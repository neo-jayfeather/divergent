#pragma once

#include <string>
#include <vector>
#include <git2.h>
#include <filesystem>
#include <unordered_map>

struct ProjectConfig {
    std::filesystem::path main_path;
    std::filesystem::path fork_path;
    std::string divergence_commit;
    std::vector<std::string> full_config; 
};

// Maybe take static members to another header? 
std::filesystem::path FindDivGitDir(const std::filesystem::path& path);

struct FileChange {
    char commit_sha[40];
    git_delta_t status; // ADDED, MODIFIED, or RENAMED
};

class DivergentEngine {
public:
    DivergentEngine(const std::string& call_path);
    ~DivergentEngine();

    // TODO: make some more abstract methods?
    std::string FindDivergenceBase();
    std::vector<std::string> DetectNewForkFiles();
    void SetMain(std::filesystem::path);
    void PullConfig();
    void WriteConfig();
    void VerboseHistory(const std::unordered_map<std::string, std::vector<FileChange>>& file_histories);
    bool GetFileHistories(git_repository* target_repo, std::unordered_map<std::string, 
        std::vector<FileChange>>& file_histories, const git_oid& divergence_oid,
        std::filesystem::path write_path);
    bool DumpFileHistoriesBinary(const std::filesystem::path write_path, std::unordered_map<std::string, std::vector<FileChange>>& file_histories);
    bool LoadFileHistoriesBinary(const std::filesystem::path read_path, std::unordered_map<std::string, std::vector<FileChange>>& file_histories);
    void PopulateFileDivergences();
    std::unordered_map<std::string, std::vector<FileChange>> fork_file_histories;
    std::unordered_map<std::string, std::vector<FileChange>> main_file_histories;
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
