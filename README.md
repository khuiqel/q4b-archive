# Q4B (very WIP)

A shoddy imitation of [PH3's P3A archive format](https://ph3at.github.io/posts/Asset-Compression/); it's a data compression tool.

You can also use this as a simple GUI for quickly compressing various files in the supported compression formats. There is also a CLI tool.

As P3A is closed-source (with the file format's header [open source](https://github.com/ph3at/p3a-format/)), I wanted to make my own version. As I do not have a large project to integrate this into, its quality is probably at least an order of magnitude below P3A. But I'm not claiming this is better; it's merely an open-source experiment. Without integrating this into a large project, it's basically just a GUI for compressing files...

### Name explanation

Q4B is named after P3A, specifically a one letter shift forward. This was inspired by [HAL 9000 being a one letter shift backward of IBM](https://en.wikipedia.org/wiki/HAL_9000#Origin_of_name)... and in whatever sleep-deprived state I was in, I thought IBM named itself after HAL, so that's why Q4B is a one letter shift forward of P3A, instead of being named "O2Z." (Also "O2Z" could easily get confused with [`-O2` and `-Oz`](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html), or so I've rationalized to myself.) That sleep-deprived state did have a nugget of rationality, because [HAL Labratories (best-known for making the Kirby games) did name itself as a one letter shift backwards from IBM](https://en.wikipedia.org/wiki/HAL_Laboratory#History), or at least that was one explanation given.

## BIG DISCLAIMER

Q4B is under active development. Do not use it for anything serious. Who knows what could happen if it tries to decode an ill-formatted archive.

## Building

0. Prerequisites: a compiler with C++23 support, CMake >=3.20
	* Linux: install [SDL dependencies](https://github.com/libsdl-org/SDL/blob/main/docs/README-linux.md)
	* Only uses SDL's Video and Render subsystems; Joystick can be enabled if you want to use a gamepad to navigate the GUI. GPU is not needed.
	* The `CMakeLists.txt` file sets the instruction set to SSE4.2 by default. If your CPU doesn't have that, change it.
	* C++23 is not strictly needed... definitely requires C++17 for `<filesystem>`, but you could probably add a [replacement library](https://github.com/gulrak/filesystem) given enough time if you want to go earlier.
0. `git clone --recursive -j8 <this repo>` (can change `-j8` to `-j<whatever>`)
0. In this project's root directory: `cmake -S . -B build`
0. Follow the OS-specific instructions below

### Linux

Compiling:

* GUI: `cmake --build build -j$(nproc) --target q4b-gui`
* CLI: `cmake --build build -j$(nproc) --target q4b`

Running:

* GUI: `./build/q4b-gui`
* CLI: `./build/q4b`

(Optional) Clean when you're done:

* `cmake --build build --target clean`

### Windows

Currently only Visual Studio is officially supported.

Compiling using CMake:

* GUI: `cmake --build build --config Release --target q4b-gui`
* CLI: `cmake --build build --config Release --target q4b`
* Running: `"build/Release/<q4b or q4b-gui>.exe"`

Compiling using Visual Studio:

1. Open `q4b.sln` (under `build/`)
1. Set build type to "Release"
1. Build `q4b-gui` and/or `q4b`
1. Run (press F5 or Ctrl+F5); defaults to `q4b-gui`, so if you want to run `q4b`, right click its Project and select "Set as Startup Project"

## Running the tests

Linux:

* Compile: `cmake --build build -j$(nproc) --target q4b-tests`
* Run: `./build/q4b-tests`

Windows:

* CMake: `cmake --build build --config Release --target q4b-tests` & `"build/Release/q4b-tests.exe"`
* Visual Studio: Build `q4b-tests` then run it

## Stuff that works

* drag and drop files to build list
* compress files using Zstd and LZ4 (or uncompressed), mostly

## Stuff that doesn't work

* compression level of the files
* hashing

## Unordered TODO list

* SDL: drop files only in the box (instead of the entire window)
* fix monitor scaling not changing window size (seems to be an ImGui issue with SDL3)
* *robustness*
* force Endianness when creating/decoding archives
* benchmarks to make graph for compression level vs time vs space
* tests
* much other stuff
* P3A compatibility mode?
* set git submodules to shallow

## Contributing

Feel free to!

I barely know what I'm doing, so don't have expectations. And development is inconsistent and may cease at any moment.

## License

GNU General Public License v3.0

### Externals' licenses

* [SDL (Simple DirectMedia Layer)](https://www.libsdl.org/): zlib
* [Dear ImGui](https://github.com/ocornut/imgui): MIT
* [googletest](https://github.com/google/googletest): BSD-3-Clause
* [cpp-optparse](https://github.com/weisslj/cpp-optparse): MIT
* [xxHash](https://github.com/Cyan4973/xxHash): BSD-2-Clause
* [Zstd](https://github.com/facebook/zstd): BSD-3-Clause or GPLv2
* [LZ4](https://github.com/lz4/lz4): BSD-2-Clause and GPLv2+
* [Noto Sans](https://notofonts.github.io/): [SIL OFL 1.1](https://openfontlicense.org/open-font-license-official-text/)

## Acknowledgments

* [PH3](https://games.ph3.at/) and [their blog](https://ph3at.github.io/)
