#pragma once

#include <git2.h>
#include <filesystem>
#include <vector>
#include <fstream>
#include <unordered_map>

struct FileChange {
    char commit_sha[40];
    char blob_oid[20];
    git_delta_t status;
};

std::filesystem::path FindDivGitDir(const std::filesystem::path& path);
bool LoadFileHistoriesBinary(const std::filesystem::path read_path, std::unordered_map<std::string, std::vector<FileChange>>& file_histories);
bool DumpFileHistoriesBinary(const std::filesystem::path write_path, std::unordered_map<std::string, std::vector<FileChange>>& file_histories);

    
class ProjectConfig{
    
    public:
        void PullConfig();
        bool WriteConfig();
        
        std::filesystem::path main_path;
        std::filesystem::path fork_path;
        std::string divergence_commit;
        std::vector<std::string> full_config; 

};