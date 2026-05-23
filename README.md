# divergent
Divergent git fork tracker and management system.
## Build
### Dependencies
- libgit2 (libgit2-dev on apt)
- pkg-config (probably some things here too!)
- build-essential (probably some things here...)
- cmake

> [!NOTE] 
> libgit2 may be bundled in the future!

### Build Process
First, generate the cmake files: `cmake -DCMAKE_BUILD_TYPE=Release ..`
Then, generate the binary: `make -j4` to build with 4 threads. To use all, use `make -j$(nproc)` or use a custom number.
