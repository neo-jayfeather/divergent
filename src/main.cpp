#include "divergent.hpp"

#include <iostream>
#include <filesystem>

// once initalized, create a .div folder
// inside div should store
// base/origin/whatever -> absolute path of other repo
// divergence commit --> DONE
// number of commits since then
// etc.

// find the diverging commit
// no idea how to do that 

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: divergent <command> [options]\n"
                  << "Commands:\n"
                  << "  init     Initialize a divergent instance (a divergence one might even call it)\n"
                  << "  set-base Set the base (project of which was forked, or things are being moved from)\n"
                  << "  scan     Map relations across forks\n";
        return 1;
    }

    std::string command = argv[1];
    DivergentEngine engine(".");

    if (command == "init") {
        if(argc == 2) {
            std::cout << "Divergent initalized tracking at base root. Add another repository to begin usage.\n";
        } else {
            engine.SetMain(argv[2]);
            std::cout << "Divergent Initalized at base (fork) and main.\n";            
            std::string base_sha = engine.FindDivergenceBase();
            std::cout << "Divergence commit: " << base_sha << "\n";
        }
        // std::cout << "Divergent initialized tracking at base root: " << base_sha << "\n";
    } else if (command == "scan") {
        auto new_files = engine.DetectNewForkFiles();
        std::cout << "Scan finished. Found " << new_files.size() << " newly added files inside fork.\n";
    } else if (command == "set-base"){
        engine.SetMain(argv[2]);
        std::string base_sha = engine.FindDivergenceBase();
        std::cout << "Divergence commit: " << base_sha << "\n";
    }
    else if (command == "git_parent"){
        std::cout << "There is a git directory at :" << FindGitDir(".") << "\n";
    }else{
        std::cout << "Not a valid command. Run with no arguments for help.\n";
    }

    return 0;
}
