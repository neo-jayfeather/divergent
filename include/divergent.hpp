#pragma once

#include <string>
#include <vector>
#include <git2.h>
#include <filesystem>

struct ProjectConfig {
    std::string upstream_remote;
    std::string fork_remote;
    std::string divergence_commit;
};


std::filesystem::path FindGitDir(const std::filesystem::path& path);


class DivergentEngine {
public:
    DivergentEngine(const std::string& call_path);
    ~DivergentEngine();

    std::string FindDivergenceBase();
    std::vector<std::string> DetectNewForkFiles();

private:
    git_repository* repo = nullptr;
    ProjectConfig config;
};
