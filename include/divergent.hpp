#pragma once

#include "files.hpp"

// files.hpp includes 
/*
#include <git2.h>
#include <filesystem>
#include <vector>
#include <fstream>
#include <unordered_map>
*/
#include <string>
#include <iostream>
#include <array>


int tree_cb(const char* root, const git_tree_entry* entry, void* payload);

class DivergentEngine {
public:
    DivergentEngine(const std::string& call_path, const std::string& other_path);
    ~DivergentEngine();

    // TODO: make some more abstract methods?
    std::string FindDivergenceBase();
    std::vector<std::string> DetectNewForkFiles();
    std::string VerboseHistory(const std::unordered_map<std::string, std::vector<FileChange>>& file_histories);
    bool GetFileHistories(git_repository* target_repo, std::unordered_map<std::string, 
        std::vector<FileChange>>& file_histories, const git_oid& divergence_oid,
        std::filesystem::path write_path);
    void PopulateFileDivergences();
    void PrintOne();
    int GetID(const std::string& path);
    const std::string& GetPath(int id);
    std::unordered_map<std::string, std::vector<FileChange>> fork_file_histories;
    std::unordered_map<std::string, std::vector<FileChange>> main_file_histories;

    std::unordered_map<std::string, std::array<unsigned char, 40>> fork_divergences;
    std::unordered_map<std::string, std::array<unsigned char, 40>> main_divergences;

    FileData fork;
    FileData main;

    
    git_repository* fork_repo = nullptr;
    git_repository* main_repo = nullptr;
    std::filesystem::path main_path;
    std::filesystem::path fork_path;
private:
    ProjectConfig config;
    
    std::unordered_map<std::string, git_oid> file_divs;
    
    struct GitOidHash {
        std::size_t operator()(const git_oid& oid) const {
            return *reinterpret_cast<const std::size_t*>(oid.id);
        }
    };

    struct GitOidEqual {
        bool operator()(const git_oid& lhs, const git_oid& rhs) const {
            return git_oid_cmp(&lhs, &rhs) == 0;
        }
    };
};
