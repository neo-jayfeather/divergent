#include "divergent.hpp"

#include <iostream>
#include <filesystem>

DivergentEngine::DivergentEngine(const std::string& call_path) {
    git_libgit2_init();

    fork_path = FindDivDir(call_path);
    std::filesystem::path temp_path(call_path);

    if (fork_path == temp_path.root_path()){
        fork_path = FindGitDir(call_path);
    }
    
    int error = git_repository_open(&repo, fork_path.c_str());

    // TODO: kind of messy error output, maybe...?
    if (error != 0) {
        auto last_error = git_error_last();
        std::string detail = last_error ? last_error->message : "Unknown file system or access error";
        throw std::runtime_error("Divergent Init Failure: " + detail);
    }

    if(std::filesystem::exists(fork_path / ".div" / "config.json")){
        // read json?
        // make own file type?
    }
}

DivergentEngine::~DivergentEngine() {
    if (repo) git_repository_free(repo);
    if (main_repo) git_repository_free(main_repo);
    git_libgit2_shutdown();
}

void DivergentEngine::SetMain(std::filesystem::path path){
    main_path = FindGitDir(path); // more likely to have git-only
    if(main_path == path.root_path()) main_path = FindDivDir(path);
    
    int error = git_repository_open(&main_repo, main_path.c_str());

    if(error != 0){
        auto last_error = git_error_last();
        std::string detail = last_error ? last_error->message: "Unknown file system or access error";
        throw std::runtime_error("Divergent Init Failure: " + detail);
    }
};


std::string DivergentEngine::FindDivergenceBase() {
    std::vector<git_oid> fork_history;
    std::vector<git_oid> main_history;

    CopyFullGitHistory(repo, fork_history);
    CopyFullGitHistory(main_repo, main_history);

    for (size_t i = 0; i < fork_history.size(); i++) {
        for (size_t j = 0; j < main_history.size(); j++) {
            if (git_oid_cmp(&fork_history[i], &main_history[j]) == 0) {
                char ret_char[65];
                git_oid_tostr(ret_char, 65, &fork_history[i]);
                
                std::string temp_str(ret_char);
                return temp_str;
            }
        }
    }

    std::cout << "No matching commit found between histories.\n";
    return ""; 
}

std::vector<std::string> DivergentEngine::DetectNewForkFiles() {
    std::vector<std::string> newly_added_files;
    // Placeholder for tree diff traversal logic later
    return newly_added_files;
}

// finds first parent directroy with a .git folder (or file...)
// if it does not exist, returns root path
std::filesystem::path FindGitDir(const std::filesystem::path& path) {
    std::filesystem::path path1 = std::filesystem::canonical(path);
    
    while(path1.has_parent_path() && path1 != path1.root_path()){
        if(std::filesystem::exists(path1 / ".git")) return path1; 
        path1 = path1.parent_path();
    }
    return path.root_path();
}

std::filesystem::path FindDivDir(const std::filesystem::path& path) {
    std::filesystem::path path1 = std::filesystem::canonical(path);
    
    while(path1.has_parent_path() && path1 != path1.root_path()){
        if(std::filesystem::exists(path1 / ".div")) return path1; 
        path1 = path1.parent_path();
    }
    return path.root_path();
}

void CopyFullGitHistory(git_repository* repo, std::vector<git_oid>& history) {
    git_revwalk* walker;
    git_revwalk_new(&walker, repo);
    git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL);
    git_revwalk_push_head(walker);

    history.reserve(1000);

    git_oid temp_oid;

    while (git_revwalk_next(&temp_oid, walker) == 0)
        history.push_back(temp_oid);

    git_revwalk_free(walker);
}