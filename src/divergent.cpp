#include "divergent.hpp"

#include <iostream>
#include <filesystem>

std::filesystem::path main_path;
std::filesystem::path fork_path;

DivergentEngine::DivergentEngine(const std::string& call_path) {
    git_libgit2_init();

    int error = git_repository_open(&repo, FindGitDir(call_path).c_str());

    // TODO: kind of messy error output, maybe...?
    if (error != 0) {
        auto last_error = git_error_last();
        std::string detail = last_error ? last_error->message : "Unknown file system or access error";
        throw std::runtime_error("Divergent Init Failure: " + detail);
    }
}

DivergentEngine::~DivergentEngine() {
    if (repo) git_repository_free(repo);
    git_libgit2_shutdown();
}

std::string DivergentEngine::FindDivergenceBase() {
    // Placeholder for merge-base logic using libgit2 later
    return "a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0"; 
}

std::vector<std::string> DivergentEngine::DetectNewForkFiles() {
    std::vector<std::string> newly_added_files;
    // Placeholder for tree diff traversal logic later
    return newly_added_files;
}

// finds first parent directroy with a .git folder (or file...)
// if it does not exist, returns root path
std::filesystem::path FindGitDir(const std::filesystem::path& path) {
    std::filesystem::path path1 = std::filesystem::absolute(path);
    
    while(path1.has_parent_path() && path1 != path1.root_path()){
        if(std::filesystem::exists(path1 / ".git")) return path1; 
        path1 = path1.parent_path();
    }
    return path.root_path();
}