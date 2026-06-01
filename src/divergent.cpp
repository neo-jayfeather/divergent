#include "divergent.hpp"
#include "files.hpp"

/*
files.hpp
#include <git2.h>
#include <filesystem>
#include <vector>
#include <fstream>
#include <iostream>
#include <unordered_map>
*/
#include <unordered_set>
#include <cstring>


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

bool DivergentEngine::GetFileHistories(git_repository* target_repo, std::unordered_map<std::string, 
    std::vector<FileChange>>& file_histories, const git_oid& divergence_oid,
    std::filesystem::path write_path) 
{
    // load from file and then return if file exists
    if(LoadFileHistoriesBinary(write_path, file_histories)) return true;

    git_revwalk* walker = nullptr;
    git_oid commit_oid;

    if (git_revwalk_new(&walker, target_repo) != 0) return false;
    
    git_revwalk_push_head(walker);
    git_revwalk_hide(walker, &divergence_oid); // Cut off baseline history
    
    // TOPOLOGICAL preserves graph shape; REVERSE moves oldest -> newest
    git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL | GIT_SORT_REVERSE);

    while (git_revwalk_next(&commit_oid, walker) == 0) {
        git_commit* commit = nullptr;
        if (git_commit_lookup(&commit, target_repo, &commit_oid) != 0) continue;

        char hex[GIT_OID_HEXSZ + 1];
        git_oid_tostr(hex, sizeof(hex), &commit_oid);
        std::string commit_sha(hex);

        git_tree* current_tree = nullptr;
        git_commit_tree(&current_tree, commit);

        git_tree* parent_tree = nullptr;
        if (git_commit_parentcount(commit) > 0) {
            git_commit* parent = nullptr;
            if (git_commit_parent(&parent, commit, 0) == 0) {
                git_commit_tree(&parent_tree, parent);
                git_commit_free(parent);
            }
        }

        git_diff* diff = nullptr;
        if (git_diff_tree_to_tree(&diff, target_repo, parent_tree, current_tree, nullptr) == 0) {
            
            size_t delta_count = git_diff_num_deltas(diff);
            for (size_t i = 0; i < delta_count; ++i) {
                const git_diff_delta* delta = git_diff_get_delta(diff, i);
                
                std::string file_path = delta->new_file.path;

                // populate structure
                FileChange change;
                std::memcpy(change.commit_sha, hex, 40);
                change.status = delta->status; // e.g., GIT_DELTA_ADDED, GIT_DELTA_MODIFIED

                file_histories[file_path].push_back(change);
            }
            git_diff_free(diff);
        }

        // Cleanup iteration handles
        if (parent_tree) git_tree_free(parent_tree);
        git_tree_free(current_tree);
        git_commit_free(commit);
    }

    git_revwalk_free(walker);
    DumpFileHistoriesBinary(write_path, file_histories);
    return true;
}

// temporary function, will be gone sooner or later
void DivergentEngine::VerboseHistory(const std::unordered_map<std::string, std::vector<FileChange>>& file_histories) {
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
    std::cout << "Counted: " << count[0] << " files with no changes since addition\n"
        << count[1] << " files with two or less changes.\n"
        << count[2] << " files with under 10 changes\n"
        << count[3] << " files with over 100 changes\n"
        << avg / total << " average # of changes of a file\n";
    std::cout << "There are " << total << " files in this repo.\n";
}


std::vector<std::string> DivergentEngine::DetectNewForkFiles() {
    std::vector<std::string> newly_added_files;
    // look through file_histories
    // find added files
    // return vector with added file paths
    return newly_added_files;
}

void DivergentEngine::PopulateFileDivergences(){
    int counter = 0; 
    int missing_counter = 0;
    // look through file blob in reverse order
    // file_path is string, changes is a vector of FileChanges
    // FORK file histories
    for(const auto& [file_path, changes] : fork_file_histories){
        for (const auto& change : changes) {
            // sha string from char array
            std::string sha_str(change.commit_sha, 40);

            // sha:path identifier
            std::string git_spec = sha_str + ":" + file_path;

            // query the repo for the blob object
            git_object* obj = nullptr;
            // FORK repo
            if (git_revparse_single(&obj, fork_repo, git_spec.c_str()) == 0) {
                //  object is actually a file blob
                if (git_object_type(obj) == GIT_OBJECT_BLOB) {
                    const git_oid* blob_oid = git_object_id(obj);

                    char blob_hex[GIT_OID_HEXSZ + 1];
                    git_oid_tostr(blob_hex, sizeof(blob_hex), blob_oid);
                    counter++;
                    // std::cout << "  Commit [" << sha_str.substr(0, 7) << "] -> Blob SHA: " << blob_hex << "\n";

                    // TODO: compare blob shas together to check if files are the same
                    // use LINEAR search (fastest, 9 elements on average)
                    // divergence MUST be in history (either the same as the divergence commit, or later
                }
                git_object_free(obj);
            } else {
                // file deletion or something like that
                // std::cout << "  Commit [" << sha_str.substr(0, 7) << "] -> File deleted or missing.\n";
                missing_counter++;
            }
        }
    }
    std::cout << "Processed " << counter << " commits\n";
    std::cout << "Missing " << missing_counter << " files\n";
}

