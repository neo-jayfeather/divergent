#pragma once

#include <git2.h>
#include <filesystem>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <cstring>

// note that commit_sha could be 20...
// something to do with unordered_map string conversions
struct FileChange {
    unsigned char commit_sha[40];
    char blob_oid[20];
    git_delta_t status;
};

struct BlobData {
    unsigned char id[20];
};

struct TreeState {
    std::unordered_map<std::string, BlobData> paths_to_blobs;
    std::vector<BlobData> blobs;
    std::vector<std::string> paths;
};

std::filesystem::path FindDivGitDir(const std::filesystem::path& path);
bool LoadFileHistoriesBinary(const std::filesystem::path& read_path, std::unordered_map<std::string, std::vector<FileChange>>& file_histories);
bool DumpFileHistoriesBinary(const std::filesystem::path& write_path, std::unordered_map<std::string, std::vector<FileChange>>& file_histories);
std::string OIDtoString(const git_oid& oid);
std::string OIDtoString(const unsigned char (*sha)[40]);

    
class ProjectConfig{
    public:
        bool PullConfig();
        bool WriteConfig();
        
        std::filesystem::path main_path;
        std::filesystem::path fork_path;
        std::string divergence_commit;
        std::vector<std::string> full_config; 

};

class FileData{
    public:
        int GetID(const std::string& path);
        const std::string& GetPath(int id);
        std::unordered_map<std::string, int> path_to_id;
        std::vector<std::string> id_to_path;
};