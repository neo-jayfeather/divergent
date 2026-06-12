// files.cpp
// methods that pertain to filesystem
// - finding .div/git
// - saving/importing files
#include "files.hpp"


// helper classless function
std::filesystem::path FindDivGitDir(const std::filesystem::path& path){
    std::filesystem::path path1 = std::filesystem::canonical(path);
    
    while(path1.has_parent_path() && path1 != path1.root_path()){
        if(std::filesystem::exists(path1 / ".div")) return path1;
        if(std::filesystem::exists(path1 / ".git")) return path1;
        path1 = path1.parent_path();
    }
    return path.root_path();
}

std::string OIDtoString(const git_oid& oid){
    char hex[GIT_OID_HEXSZ + 1];
    git_oid_tostr(hex, sizeof(hex), &oid);
    return hex;
}

std::string OIDtoString(const unsigned char (*sha)[40]){
    git_oid temp_oid;
    std::memcpy(temp_oid.id, sha, 20);
    char hex[GIT_OID_HEXSZ + 1];
    git_oid_tostr(hex, sizeof(hex), &temp_oid);
    return hex;
}

bool ProjectConfig::PullConfig(){
    if(std::filesystem::exists(fork_path / ".div" / "div.config")){
        std::ifstream cfg_file(fork_path / ".div" / "div.config");
        std::string temp_str;

        std::vector<std::string> cfg_lines;
        cfg_lines.reserve(5);
        for(int i = 0; i < 2; i++){
            getline(cfg_file, temp_str);
            cfg_lines.emplace_back(temp_str);
        }
        cfg_file.close();

        main_path = cfg_lines[0];
        divergence_commit = cfg_lines[1];

        return true;
    }
    return WriteConfig();
}

bool ProjectConfig::WriteConfig(){
    if(!std::filesystem::exists(fork_path / ".div")) std::filesystem::create_directory(fork_path / ".div");

    std::ofstream cfg_file(fork_path / ".div" / "div.config");
    
    if (cfg_file.is_open()) {
        // config data to the file
        if(main_path == "" || divergence_commit == "") return false;
        cfg_file << main_path.string() << "\n";
        cfg_file << divergence_commit << std::endl;

        cfg_file.close();
        return true;
    }
    return false;
}

bool LoadFileHistoriesBinary(const std::filesystem::path& read_path, std::unordered_map<std::string, std::vector<FileChange>>& file_histories) {
    std::ifstream in(read_path, std::ios::in | std::ios::binary);
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

bool DumpFileHistoriesBinary(const std::filesystem::path& write_path, std::unordered_map<std::string, std::vector<FileChange>>& file_histories) {
    if(!std::filesystem::exists(write_path)) std::filesystem::create_directories(write_path.parent_path());
    std::ofstream out(write_path, std::ios::out | std::ios::binary | std::ios::trunc);
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