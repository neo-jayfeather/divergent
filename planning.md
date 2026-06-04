# Planning

### CLI Functions
#### Init
```divergent init [dir]``` - takes directory and initalizes in current directory, `main` is `dir`.
- `fork` is initalization dir
- fails if there is no argument OR one or more of the directories do not exist
- `.git` folder **cannot** be in root 
- uses canonical paths, symlinks and such should be found regularly
- only tested on drive with OS, external drives may fail indefinitly if there is no git folder there
- (note to self, add that please :D)

In addition, this should scan the directory and create a mapping of these things. This may take some time.
- Find the divergence commit (0.5s)
- Map out git histories to two different unordered maps that are vectors. (60s)
- Create `main` and `fork` folders in `.div`folder in `fork`.
- Find individual file histories. (lots of time)
- Find individaul file divergences. (< 10s)
- Map individual files with some sort of function and such parsing KEEP originals, MAP changed.
- Create some sort of mapping.

#### Stats 
Print out stats.
It will show the following:
- Divergence commit hash (8 char/40 char hash)
- \# Number of files 
- File changes (\#)
- Files that are the same, files that have changed, files with 100+ changes


#### Future Implementations
##### Delete
Deletes all saved data, folders, returns to state before `init`.
#### .json parser 
json combiner of some sorts that can be used appropriately to help with the aid of the creation of this.
#### ignore
Command to add types of files to ignore/dirs, cam be configured like a `.gitignore` (will be called `.divignore`).
#### multithreading (concept)
Multithread everything. Why is it so slow? Can break down into better, smaller subproblems. Use SIMD optimization. Furthermore, just help the people using this out. 