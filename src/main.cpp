#include "divergent.hpp"
#include "files.hpp"

// within div folder store:
// base/origin/whatever -> absolute path of other repo
// number of commits since then
// etc.

// TODO:
// CODE - .json parser for .json files - for json tracking & merging, but not necessary 
// CONFIG - ignore certain folders
// CODE - track vars

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

// STEP TWO
// function comparisons
// find identical functions
//      return signature
//      parameters
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
// translation map
// STEP FIVE
// apply translations
// STEP SIX
// merge :D

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: divergent <command> [options]\n"
                  << "Commands:\n"
                  << "  init     Initialize a divergent instance (a divergence one might even call it)\n"
                  << "  scan     Map relations across forks\n";
        return 1;
    }

    std::string command = argv[1];
    // if trying to init without a second path, error
    if(command == "init" && argc < 3) return 1;
    // TODO pull from config
    DivergentEngine engine(".", argv[2]);
    ProjectConfig config;
    // engine should host functions
    // config can also host functions but for data saving/retreival 
    // config should save non-ephemeral data that will be needed later

    if (command == "init") {
        // find saved config data, if any
        config.PullConfig(); // please see into filetype/filesystem for this... 
        // find divergence base (fast)
        std::cout << "Divergent Initalized at base (fork) and main.\n";

        std::string temp_sha = engine.FindDivergenceBase();

        std::cout << "Divergence commit: " << temp_sha << "\n";
        
        git_oid oid;
        git_oid_fromstr(&oid, temp_sha.c_str());
        // find file histories for fork (and main)
        // what if these became a struct of some sort...? 
        // who knows -- pretty slow, ~30-60s per 10k
        // COULD be parallelized...
        engine.GetFileHistories(engine.fork_repo, engine.fork_file_histories, oid, engine.fork_path / ".div" / "fork" / "fileHis.div");
        std::cout << engine.VerboseHistory(engine.fork_file_histories);
        engine.GetFileHistories(engine.main_repo, engine.main_file_histories, oid, engine.fork_path / ".div" / "main" / "fileHis.div");
        std::cout << engine.VerboseHistory(engine.main_file_histories);
        
        // find individual file divergences (does NOT save)
        // probably also some multithreading capability here :D
        // VERY ver yVERY slow right now
        // engine.PopulateFileDivergences();        
    } else if (command == "scan") {
        // require some previous scan or something, idk
        // maybe delete this or something, idk how i plan on updating this
        auto new_files = engine.DetectNewForkFiles();
        std::cout << "Scan finished. Found " << new_files.size() << " newly added files inside fork.\n";
    } else if (command == "stat"){
        // print stats
        // how many same
        // how mayn diff
        // how many commits
        // how many divergence is last change
        // how many no divergence 
        // make this faster/cpu mark? 
        // nobody knows how this works do they
        // save stats in config file?
        // can use space separation or something for config, kind of human readable but not too much so
        // no need to obfuscate anything else though
        // nobody really knows how to use this either, it's ok though
    }
    else{
        // maybe earlier exit better, who knows
        std::cout << "Not a valid command. Run with no arguments for help.\n";
    }

    return 0;
}
