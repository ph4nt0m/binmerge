# binmerge
Combining several binary files into one can be easily done by using `cat` (Unix) or `copy /b` (Windows). But in some special cases, the input files might not be disjoint but instead overlap by a certain amount. For example, when dealing with several `.ts` files that result from a split up TV recording, this situation is quite common. `binmerge` tries to handle these cases by trying to determine the overlapping areas.

## Building from Source
Clone the repository into a local folder:
```
git clone --recursive https://github.com/ph4nt0m/binmerge.git
```
Note that the `--recursive` option is important to clone all additional submodules that are used by this project. Next, create a `build` folder and run [CMake](https://cmake.org/) from there: 
```
cd binmerge
mkdir build
cd build
cmake ..
cmake --build .
```
After that, you'll find the executable in `binmerge/bin/`.

## Basic Usage
```
binmerge <file1> <file2> ... <fileN>
```

## How It Works
Based on the given file sequence, `binmerge` will try to find overlapping areas between any two files by checking if the last 20 bytes of one file occur in the next file. If this search has been successful, the two files are assumed to be overlapping and will be merged accordingly (for information purposes, the overlapping areas will be compared byte-wise to print a matching percentage).

Should the pattern search not succeed, a simple concatenation will be performed instead.
