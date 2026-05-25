#include "divergent.hpp"

#include <unordered_set>
#include <iostream>
#include <filesystem>
#include <fstream>


DivergentEngine::DivergentEngine(const std::string& call_path) {
    git_libgit2_init();

    fork_path = FindDivGitDir(call_path);
    
    int error = git_repository_open(&repo, fork_path.c_str());

    // TODO: kind of messy error output, maybe...?
    if (error != 0) {
        auto last_error = git_error_last();
        std::string detail = last_error ? last_error->message : "Unknown file system or access error";
        throw std::runtime_error("Divergent Init Failure: " + detail);
    }
}

DivergentEngine::~DivergentEngine() {
    if (repo) git_repository_free(repo);
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
        // 
        if(config.main_path == "" || config.divergence_commit == "") return;
        cfg_file << config.main_path.string() << std::endl;
        cfg_file << config.divergence_commit << std::endl;

        cfg_file.close();
    } else std::cout << "Failed to write config file.";

}

std::string DivergentEngine::FindDivergenceBase() {
    if(!config.divergence_commit.empty()) return config.divergence_commit;

    git_revwalk* main_walker = nullptr;
    git_revwalk* fork_walker = nullptr;
    git_oid main_tip, fork_tip;

    if (git_reference_name_to_id(&main_tip, main_repo, "HEAD") != 0 ||
        git_reference_name_to_id(&fork_tip, repo, "HEAD") != 0)
        return "";

    git_revwalk_new(&main_walker, main_repo);
    git_revwalk_push(main_walker, &main_tip);

    git_revwalk_new(&fork_walker, repo);
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