#include "divergent.hpp"
#include "files.hpp"

/*
files.hpp
#include <git2.h>
#include <filesystem>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <cstring>
*/

#include <unordered_set>
#include <array>
// timing only
#include <chrono>

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
                    config.divergence_commit = OIDtoString(commit_m);
                    break;
                }
                visited_commits.insert(commit_m);
            } else main_active = false;
        }

        if (fork_active) {
            if (git_revwalk_next(&commit_f, fork_walker) == 0) {
                if (visited_commits.count(commit_f) > 0) {
                    config.divergence_commit = OIDtoString(commit_f);
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

    // vector cache list for revwalk
    std::vector<git_oid> commit_list;
    git_revwalk* walker;
    git_revwalk_new(&walker, target_repo);
    git_revwalk_push_head(walker);
    git_revwalk_hide(walker, &divergence_oid);
    git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL | GIT_SORT_REVERSE);
    
    git_oid oid;
    while(git_revwalk_next(&oid, walker) == 0) {
        commit_list.push_back(oid);
    }
    git_revwalk_free(walker);

    TreeState last_tree;
    size_t total = commit_list.size();
    
    std::cout << "Processing " << total << " commits..." << std::endl;

    std::array<unsigned long, 4> times = {0};
    std::chrono::_V2::system_clock::time_point start;

    // reserve ahead
    file_histories.reserve(total);

    for (size_t i = 0; i < total; ++i) {
        start = std::chrono::high_resolution_clock::now();

        git_commit* commit;
        git_commit_lookup(&commit, target_repo, &commit_list[i]);

        times[0] += (std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start)).count();
        
        start = std::chrono::high_resolution_clock::now();

        git_tree* tree;
        git_commit_tree(&tree, commit);

        times[1] += (std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start)).count();

        start = std::chrono::high_resolution_clock::now();

        TreeState current_tree;
        git_tree_walk(tree, GIT_TREEWALK_PRE, tree_cb, &current_tree);

        times[2] += (std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start)).count();

        start = std::chrono::high_resolution_clock::now();

        const auto last_tree_end = last_tree.paths_to_blobs.end();

        for (const auto& [path, blob] : current_tree.paths_to_blobs) {
            auto it = last_tree.paths_to_blobs.find(path);
            
            if (it == last_tree_end || std::memcmp(it->second.id, blob.id, 20) != 0) {
                FileChange change;
                std::memcpy(change.commit_sha, commit_list[i].id, 20);
                std::memcpy(change.blob_oid, blob.id, 20);
                change.status = GIT_DELTA_MODIFIED; 
                
                file_histories[path].push_back(change);
            }
        }

        times[3] += (std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start)).count();

        last_tree = std::move(current_tree);
        git_tree_free(tree);
        git_commit_free(commit);

        if (i % 100 == 0 || i == total - 1) {
            float progress = static_cast<float>(i) / total;
            int bar_width = 40;
            std::cout << "\r[";
            for(int j = 0; j < bar_width; ++j) {
                std::cout << (j < bar_width * progress ? '=' : ' ');
            }
            std::cout << "] " << (int)(progress * 100) << "% " << std::flush;
        }
    }
    std::cout << "\n" << times[0] / total << " - 1/iter\n"
        << times[1] / total << " - 2/iter\n"
        << times[2] / total << " - 3/iter\n"
        << times[3] / total << " - 4/iter\n"
        << times[0] << " " << times[1] << " " << times[2] << " " << times[3] << std::endl;

    return DumpFileHistoriesBinary(write_path, file_histories);
}

void DivergentEngine::PopulateFileDivergences() {
    for (const auto& [file_path, fork_changes] : fork_file_histories) {
        if (main_file_histories.find(file_path) == main_file_histories.end()) continue;

        const auto& main_changes = main_file_histories.at(file_path);

        auto f_it = fork_changes.rbegin();
        auto m_it = main_changes.rbegin();

        // finds first divergence (most efficient for now...)
        while (f_it != fork_changes.rend() && m_it != main_changes.rend()) {
            if (std::memcmp(f_it->blob_oid, m_it->blob_oid, 20) == 0) {
                ++f_it;
                ++m_it;
            } else {
                // char hex[GIT_OID_HEXSZ + 1];
                // git_oid temp_oid;
                // std::memcpy(temp_oid.id, f_it->commit_sha, 20);
                // git_oid_tostr(hex, sizeof(hex), &temp_oid);

                std::copy(std::begin(f_it->commit_sha), std::end(f_it->commit_sha), fork_divergences[file_path].begin());

                break;
            }
        }
    }
}

// temporary function, will be gone sooner or later (maybe not so temp overall)
// maybe string is slower overall here
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

// cleanup functions should do things
std::vector<std::string> DivergentEngine::DetectNewForkFiles() {
    std::vector<std::string> newly_added_files;
    // look through file_histories
    // find added files
    // return vector with added file paths
    return newly_added_files;
}


int tree_cb(const char* root, const git_tree_entry* entry, void* payload) {
    if (git_tree_entry_type(entry) != GIT_OBJECT_BLOB) return 0;

    auto* state = static_cast<TreeState*>(payload);
    std::string path = std::string(root) + git_tree_entry_name(entry);
    
    BlobData blob;
    std::memcpy(blob.id, git_tree_entry_id(entry)->id, 20);
    state->paths_to_blobs[path] = blob;
    // state->paths.push_back(std::move(path));
    // state->blobs.push_back(blob);
    return 0;
}


void DivergentEngine::PrintOne(){
    size_t counter = 0;
    for(const auto& [key, value] : fork_file_histories){
        if(value.size() == 1){

            counter++;
            std::cout << key << "\n";
            std::cout << OIDtoString(&value[0].commit_sha) << "\n";
            counter++;
        }
        if(counter > 10) break;
    }
}