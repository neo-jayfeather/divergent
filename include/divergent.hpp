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


std::filesystem::path FindDivGitDir(const std::filesystem::path& path);
void CopyFullGitHistory(git_repository* repo, std::vector<git_oid>& history);

struct FileChange {
    char commit_sha[40];
    git_delta_t status; // ADDED, MODIFIED, or RENAMED
};

class DivergentEngine {
public:
    DivergentEngine(const std::string& call_path);
    ~DivergentEngine();

    std::string FindDivergenceBase();
    std::vector<std::string> DetectNewForkFiles();
    void SetMain(std::filesystem::path);
    void PullConfig();
    void WriteConfig();
    void CatalogFileHistories(const git_oid& divergence_oid);
    bool CatalogFileHistories(std::unordered_map<std::string, std::vector<FileChange>>& file_histories, 
        const git_oid& divergence_oid);

    void VerboseHistory();
    bool DumpFileHistoriesBinary();
    bool LoadFileHistoriesBinary();
    
private:
    git_repository* fork_repo = nullptr;
    git_repository* main_repo = nullptr;
    ProjectConfig config;
    std::filesystem::path main_path;
    std::filesystem::path fork_path;
    
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
    
    std::unordered_map<std::string, std::vector<FileChange>> file_histories;
};
