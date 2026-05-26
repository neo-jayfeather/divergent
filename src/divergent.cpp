#include "divergent.hpp"

#include <unordered_set>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cstring>


DivergentEngine::DivergentEngine(const std::string& call_path) {
    git_libgit2_init();

    fork_path = FindDivGitDir(call_path);
    
    int error = git_repository_open(&fork_repo, fork_path.c_str());

    // TODO: kind of messy error output, maybe...?
    if (error != 0) {
        auto last_error = git_error_last();
        std::string detail = last_error ? last_error->message : "Unknown file system or access error";
        throw std::runtime_error("Divergent Init Failure: " + detail);
    }
}

DivergentEngine::~DivergentEngine() {
    if (fork_repo) git_repository_free(fork_repo);
    if (main_repo) git_repository_free(main_repo);
    git_libgit2_shutdown();
}

void DivergentEngine::SetMain(std::filesystem::path path){
    main_path = FindDivGitDir(path);
    config.main_path = main_path;
    
    int error = git_repository_open(&main_repo, main_path.c_str());

    if(error != 0){
        auto last_error = git_error_last();
        std::string detail = last_error ? last_error->message: "Unknown file system or access error";
        throw std::runtime_error("Divergent Init Failure: " + detail);
    }
};

void DivergentEngine::PullConfig(){
    if(std::filesystem::exists(fork_path / ".div" / "div.config")){
        std::ifstream cfg_file(fork_path / ".div" / "div.config");
        std::string temp_str;

        getline(cfg_file, temp_str);
        config.main_path = temp_str;
        getline(cfg_file, temp_str);
        config.divergence_commit = temp_str;
        // read file...

        cfg_file.close();
    } else {
        WriteConfig();
    }
}

void DivergentEngine::WriteConfig(){
    if(!std::filesystem::exists(fork_path / ".div")) std::filesystem::create_directory(fork_path / ".div");

    std::ofstream cfg_file(fork_path / ".div" / "div.config");
    
    if (cfg_file.is_open()) {
        // config data to the file
        if(config.main_path == "" || config.divergence_commit == "") return;
        cfg_file << config.main_path.string() << '\n"';
        cfg_file << config.divergence_commit << std::endl;

        cfg_file.close();
    } else std::cout << "Failed to write config file.";

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

void DivergentEngine::CatalogFileHistories(const git_oid& divergence_oid){
    CatalogFileHistories(file_histories, divergence_oid);
}

bool DivergentEngine::CatalogFileHistories(std::unordered_map<std::string, std::vector<FileChange>>& file_histories, const git_oid& divergence_oid) 
{
    if(LoadFileHistoriesBinary()) return true;

    git_revwalk* walker = nullptr;
    git_oid commit_oid;

    if (git_revwalk_new(&walker, main_repo) != 0) return false;
    
    git_revwalk_push_head(walker);
    git_revwalk_hide(walker, &divergence_oid); // Cut off baseline history
    
    // TOPOLOGICAL preserves graph shape; REVERSE moves oldest -> newest
    git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL | GIT_SORT_REVERSE);

    while (git_revwalk_next(&commit_oid, walker) == 0) {
        git_commit* commit = nullptr;
        if (git_commit_lookup(&commit, main_repo, &commit_oid) != 0) continue;

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
        if (git_diff_tree_to_tree(&diff, fork_repo, parent_tree, current_tree, nullptr) == 0) {
            
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
    DumpFileHistoriesBinary();
    return true;
}

void DivergentEngine::VerboseHistory(){
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

bool DivergentEngine::DumpFileHistoriesBinary() {
    std::ofstream out(fork_path / ".div" / "fileHis.div", std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;

    size_t total_files = file_histories.size();
    out.write(reinterpret_cast<const char*>(&total_files), sizeof(total_files));

    for (const auto& [file_path, changes] : file_histories) {
        size_t path_len = file_path.size();
        out.write(reinterpret_cast<const char*>(&path_len), sizeof(path_len));
        out.write(file_path.data(), path_len);

        size_t change_count = changes.size();
        out.write(reinterpret_cast<const char*>(&change_count), sizeof(change_count));
        
        if (change_count > 0) {
            out.write(reinterpret_cast<const char*>(changes.data()), change_count * sizeof(FileChange));
        }
    }
    return true;
}

bool DivergentEngine::LoadFileHistoriesBinary() {
    std::ifstream in(fork_path / ".div" / "fileHis.div", std::ios::in | std::ios::binary);
    if (!in.is_open()) return false;

    file_histories.clear();

    size_t total_files = 0;
    in.read(reinterpret_cast<char*>(&total_files), sizeof(total_files));
    if (!in) return false;

    for (size_t i = 0; i < total_files; ++i) {
        size_t path_len = 0;
        in.read(reinterpret_cast<char*>(&path_len), sizeof(path_len));
        
        std::string file_path(path_len, '\0');
        in.read(&file_path[0], path_len);

        size_t change_count = 0;
        in.read(reinterpret_cast<char*>(&change_count), sizeof(change_count));

        std::vector<FileChange> changes;
        if (change_count > 0) {
            changes.resize(change_count);
            
            in.read(reinterpret_cast<char*>(changes.data()), change_count * sizeof(FileChange));
        }

        file_histories[file_path] = std::move(changes);
    }
    return true;
}

std::vector<std::string> DivergentEngine::DetectNewForkFiles() {
    std::vector<std::string> newly_added_files;
    // Placeholder for tree diff traversal logic later
    return newly_added_files;
}

std::filesystem::path FindDivGitDir(const std::filesystem::path& path){
    std::filesystem::path path1 = std::filesystem::canonical(path);
    
    while(path1.has_parent_path() && path1 != path1.root_path()){
        if(std::filesystem::exists(path1 / ".div")) return path1;
        if(std::filesystem::exists(path1 / ".git")) return path1;
        path1 = path1.parent_path();
    }
    return path.root_path();
}

void CopyFullGitHistory(git_repository* repo, std::vector<git_oid>& history) {
    git_revwalk* walker;
    git_revwalk_new(&walker, repo);
    git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL);
    git_revwalk_push_head(walker);

    history.reserve(5000);

    git_oid temp_oid;

    while (git_revwalk_next(&temp_oid, walker) == 0)
        history.push_back(temp_oid);

    git_revwalk_free(walker);
}