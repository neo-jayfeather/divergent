#include "divergent.hpp"

#include <iostream>
#include <filesystem>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: divergent <command> [options]\n"
                  << "Commands:\n"
                  << "  init     Initialize a divergent instance (a divergence one might even call it)\n"
                  << "  scan     Map relations across forks\n";
        return 1;
    }

    std::string command = argv[1];
    DivergentEngine engine(".");

    if (command == "init") {
        std::string base_sha = engine.FindDivergenceBase();
        std::cout << "Divergent initialized tracking at base root: " << base_sha << "\n";
    } else if (command == "scan") {
        auto new_files = engine.DetectNewForkFiles();
        std::cout << "Scan finished. Found " << new_files.size() << " newly added files inside fork.\n";
    }else if (command == "git_parent"){
        std::cout << "There is a git directory at :" << FindGitDir(".") << "\n";
    }else{
        std::cout << "Not a valid command. Run with no arguments for help.\n";
    }

    return 0;
}
