#include "divergent.hpp"
#include "files.hpp"

// once initalized, create a .div folder
// inside div should store
// base/origin/whatever -> absolute path of other repo
// divergence commit --> DONE
// number of commits since then
// etc.

// TODO:
// CODE - .json parser for .json files - for json tracking & merging, but not necessary 
// CONFIG - ignore certain folders
// CODE - track vars
// from file divergences:
// find number of changes to each file since divergence
// resort based on that
// this should be part of initalization too!
// TODO by 5/26 NIGHT or 5/27 MORNING.
// okokokokokoko

// figure out tracking schema (database...?)
// track variables, etc. with a hash (?) --> how do i associate something that changes?
// track functions with return and param sig(s), etc.
// abc --> bcd, etc. 
// types of files tracking (.cpp, .txt, .hpp, .c, .h, etc.)
// wow this is complciated

// STEP ONE
// file to file comparison
// which files map to which, which don't exist?
// which are identical or near identical
// file comparison percentage (git api?)

// STEP TWO
// function comparisons
// find identical functions
//      return signature
//      parameters
//      inner i/o
//      ast map?
//      use diffs to find if structure has changed
// function expansions
// function compressions

// STEP THREE
// data structure/variable comparisons
// find identical variables, etc.
// data type, changes, names, etc.

// STEP THREE
// misc comparisons

// STEP FOUR 
// translation  map

// STEP FIVE
// apply translations to such things

// STEP SIX
// merge :D

// STEP SEVEN
// auto build

// STEP EIGHT
// ai summary?

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
    if(command == "init" && argc < 3) return 1;
    // pull from config later... 
    DivergentEngine engine(".", argv[2]);
    ProjectConfig config;


    if (command == "init") {
        config.PullConfig();
        std::cout << "Divergent Initalized at base (fork) and main.\n";
        std::cout << "Divergence commit: " << engine.FindDivergenceBase() << "\n";
        std::string temp_sha = engine.FindDivergenceBase();
        git_oid oid;
        git_oid_fromstr(&oid, temp_sha.c_str());
        engine.GetFileHistories(engine.fork_repo, engine.fork_file_histories, oid, engine.fork_path / ".div" / "fork" / "fileHis.div");
        engine.GetFileHistories(engine.main_repo, engine.main_file_histories, oid, engine.fork_path / ".div" / "main" / "fileHis.div");
        engine.VerboseHistory(engine.fork_file_histories);
        engine.VerboseHistory(engine.main_file_histories);
        engine.PopulateFileDivergences();        
    } else if (command == "scan") {
        auto new_files = engine.DetectNewForkFiles();
        std::cout << "Scan finished. Found " << new_files.size() << " newly added files inside fork.\n";
    }
    else if (command == "git_parent"){
        std::cout << "There is a git directory at :" << FindDivGitDir(".") << "\n";
    }else{
        std::cout << "Not a valid command. Run with no arguments for help.\n";
    }

    return 0;
}
