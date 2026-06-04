#include "divergent.hpp"
#include "files.hpp"

/*
files.hpp
#include <git2.h>
#include <filesystem>
#include <vector>
#include <fstream>
#include <unordered_map>
*/
// unordered_set may be converted to map....
#include <unordered_set>
#include <cstring>
#include <map>

// these revwalks man...

struct BlobData {
    unsigned char id[20];
};

struct TreeState {
    std::unordered_map<std::string, BlobData> paths_to_blobs;
};


DivergentEngine::DivergentEngine(const std::string& call_path, const std::string& other_path) {
    git_libgit2_init();

    fork_path = FindDivGitDir(call_path);
    main_path = FindDivGitDir(other_path);
    
    int error0 = git_repository_open(&fork_repo, fork_path.c_str());
    int error1 = git_repository_open(&main_repo, main_path.c_str());

    // TODO: kind of messy error output, maybe...?
    if (error0 != 0 || error1 != 0) {
        auto last_error = git_error_last();
        std::string detail = last_error ? last_error->message : "Unknown file system or access error";
        throw std::runtime_error("Divergent Init Failure: " + detail);
    }
    // only set to config if they are both good, valid paths
    config.fork_path = fork_path;
    config.main_path = main_path;
}

DivergentEngine::~DivergentEngine() {
    if (fork_repo) git_repository_free(fork_repo);
    if (main_repo) git_repository_free(main_repo);
    git_libgit2_shutdown();
}

std::string DivergentEngine::FindDivergenceBase() {
    if(!config.divergence_commit.empty()) return config.divergence_commit;

    git_revwalk* main_walker = nullptr;
    git_revwalk* fork_walker = nullptr;
    git_oid main_tip, fork_tip;

    // push refs
    if (git_reference_name_to_id(&main_tip, main_repo, "HEAD") != 0 ||
        git_reference_name_to_id(&fork_tip, fork_repo, "HEAD") != 0)
        return "";
    
    // revwalks
    git_revwalk_new(&main_walker, main_repo);
    git_revwalk_push(main_walker, &main_tip);

    git_revwalk_new(&fork_walker, fork_repo);
    git_revwalk_push(fork_walker, &fork_tip);

    std::unordered_set<git_oid, GitOidHash, GitOidEqual> visited_commits;
    visited_commits.reserve(10000); 

    git_oid commit_m, commit_f;
    bool main_active = true;
    bool fork_active = true;

    while (main_active || fork_active) {
        if (main_active) {
            if (git_revwalk_next(&commit_m, main_walker) == 0) {
                if (visited_commits.count(commit_m) > 0) {
                    char hex[GIT_OID_HEXSZ + 1];
                    git_oid_tostr(hex, sizeof(hex), &commit_m);
                    config.divergence_commit = hex;
                    break;
                }
                visited_commits.insert(commit_m);
            } else main_active = false;
        }

        if (fork_active) {
            if (git_revwalk_next(&commit_f, fork_walker) == 0) {
                if (visited_commits.count(commit_f) > 0) {
                    char hex[GIT_OID_HEXSZ + 1];
                    git_oid_tostr(hex, sizeof(hex), &commit_f);
                    config.divergence_commit = hex;
                    break;
                }
                visited_commits.insert(commit_f);
            } else fork_active = false;
        }
    }

    git_revwalk_free(main_walker);
    git_revwalk_free(fork_walker);

    return config.divergence_commit;
}

// this is really stupid btw
// i don't even know if this works
bool DivergentEngine::GetFileHistories(git_repository* target_repo, std::unordered_map<std::string, std::vector<FileChange>>& file_histories, 
    const git_oid& divergence_oid, std::filesystem::path write_path) 
{
    if (LoadFileHistoriesBinary(write_path, file_histories)) return true;

    // count total commits (move to function later)
    git_revwalk* counter_walker;
    git_revwalk_new(&counter_walker, target_repo);
    git_revwalk_push_head(counter_walker);
    git_revwalk_hide(counter_walker, &divergence_oid);
    size_t total_commits = 0;
    git_oid temp_oid;
    while(git_revwalk_next(&temp_oid, counter_walker) == 0) total_commits++;
    git_revwalk_free(counter_walker);



    git_revwalk* walker;
    git_revwalk_new(&walker, target_repo);
    git_revwalk_push_head(walker);
    git_revwalk_hide(walker, &divergence_oid);
    git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL | GIT_SORT_REVERSE);

    git_oid commit_oid;
    TreeState last_tree;
    size_t commit_count = 0;

    std::cout << "Processing " << total_commits << " commits..." << std::endl;

    while (git_revwalk_next(&commit_oid, walker) == 0) {
        git_commit* commit;
        git_commit_lookup(&commit, target_repo, &commit_oid);
        
        git_tree* tree;
        git_commit_tree(&tree, commit);

        TreeState current_tree;
        git_tree_walk(tree, GIT_TREEWALK_PRE, tree_cb, &current_tree);

        for (const auto& [path, blob] : current_tree.paths_to_blobs) {
            auto it = last_tree.paths_to_blobs.find(path);
            
            // if file is new, or blob hash changed: record change
            // memcmp is for optimization
            if (it == last_tree.paths_to_blobs.end() || std::memcmp(it->second.id, blob.id, 20) != 0) {
                FileChange change;
                std::memcpy(change.commit_sha, commit_oid.id, 20);
                std::memcpy(change.blob_oid, blob.id, 20);
                change.status = GIT_DELTA_MODIFIED; 
                file_histories[path].push_back(change);
            }
        }

        last_tree = std::move(current_tree);
        git_tree_free(tree);
        git_commit_free(commit);

        // progress bar
        if (++commit_count % 100 == 0 || commit_count == total_commits) {
            float progress = static_cast<float>(commit_count) / total_commits;
            int bar_width = 40;
            std::cout << "\r[";
            for(int i = 0; i < bar_width; ++i) {
                if(i < bar_width * progress) std::cout << "=";
                else std::cout << " ";
            }
            std::cout << "] " << (int)(progress * 100) << "% " << std::flush;
        }
    }
    
    std::cout << std::endl;
    git_revwalk_free(walker);
    return DumpFileHistoriesBinary(write_path, file_histories);
}

// temporary function, will be gone sooner or later (maybe not so temp overall)
std::string DivergentEngine::VerboseHistory(const std::unordered_map<std::string, std::vector<FileChange>>& file_histories) {
    int count[10] = {0};
    int total = 0;
    long avg = 0;
    for(const auto& [key, value] : file_histories){
        total++;
        if(value.size() == 1) count[0] ++;
        else if(value.size() < 3) count[1] ++;
        else if(value.size() < 11) count[2] ++;
        if(value.size() > 100) count[3] ++;
        avg += value.size();
    }

    std::string output = "Counted: " + std::to_string(count[0]) + " files with no changes since addition\n"
        + std::to_string(count[1]) + " files with two or less changes.\n"
        + std::to_string(count[2]) + " files with under 10 changes\n"
        + std::to_string(count[3]) + " files with over 100 changes\n"
        + std::to_string(avg / total) + " average # of changes of a file\n";
    
    return output;
}


std::vector<std::string> DivergentEngine::DetectNewForkFiles() {
    std::vector<std::string> newly_added_files;
    // look through file_histories
    // find added files
    // return vector with added file paths
    return newly_added_files;
}

// extremely slow...
// no idea why yet...
// this has been running for an insane amount of time

void DivergentEngine::PopulateFileDivergences() {
    // this takes too long
    // idk why
    // find # of changes after divergence
    // delete prior changes to divergence
    int divergence_found = 0;
    // int missing_counter = 0;

    for (const auto& [file_path, fork_changes] : fork_file_histories) {
        if (main_file_histories.find(file_path) == main_file_histories.end()) continue;

        const auto& main_changes = main_file_histories.at(file_path);

        for (const auto& fork_change : fork_changes) {
            for (const auto& main_change : main_changes) {
                if (std::memcmp(fork_change.blob_oid, main_change.blob_oid, 20) == 0) {

                    // Found a match: they shared this file content at some point
                    // The 'fork_change' is the point of divergence
                    divergence_found++;
                    goto next_file;
                }
            }
        }
        next_file:;
    }
    std::cout << "Files with identified divergence point: " << divergence_found << "\n";
}



// Callback for git_tree_walk
int tree_cb(const char* root, const git_tree_entry* entry, void* payload) {
    auto* state = static_cast<TreeState*>(payload);
    std::string path = std::string(root) + git_tree_entry_name(entry);
    
    if (git_tree_entry_type(entry) == GIT_OBJECT_BLOB) {
        BlobData blob;
        // Copy the raw 20 bytes directly
        std::memcpy(blob.id, git_tree_entry_id(entry)->id, 20);
        state->paths_to_blobs[path] = blob;
    }
    return 0;
}