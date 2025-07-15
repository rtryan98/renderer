# Renderer
Research / Toy Renderer

## Table of Contents
- [Building](#building)
    - [Requirements](#requirements)
- [Features](#features)
    - [Graphics Techniques](#graphics-techniques)
- [Legal](#legal)

## Building
Building Renderer requires the following:
- Windows 11
- CMake 3.28

The steps for Visual Studio 2022 are:
1. Clone the repository: `git clone https://github.com/rtryan98/renderer.git --recurse-submodules --shallow-submodules`.
2. Call CMake inside the cloned repository: `cmake -B build -G "Visual Studio 17 2022"`.
3. Open the solution file: `cmake --open build`.
4. Build the project inside Visual Studio.

Windows 10 and older Visual Studio versions are not tested and may not work. Build was tested using MSVC via both `cl.exe` and `clang-cl.exe`.

## Features
### Graphics Techniques
- Inverse Fast Fourier Transform based Ocean Simulation
    - Utilizes oceanographic spectra presented in C. Horvath's 'Empirical directional wave spectra for computer graphics' [\[DOI\]](https://dl.acm.org/doi/10.1145/2791261.2791267).
    - The synthesized spectra are packed together to halve the amount of IFFTs required and thus double the performance of the simulation.

## Legal
This project is licensed under the MIT license.

Beyond this, it also makes use of several third party libraries and assets, each of which have their respective licensing information stored in their repositories.
Currently the following third party libraries and assets are used:
- [enkiTS](https://github.com/dougbinks/enkiTS)
- [fastgltf](https://github.com/spnda/fastgltf)
- [glTF-Sample-Assets](https://github.com/KhronosGroup/glTF-Sample-Assets)
- [Dear Imgui](https://github.com/ocornut/imgui)
- [MikkTSpace](https://github.com/mmikk/MikkTSpace)
- [RHI](https://github.com/rtryan98/rhi)
- [spdlog](https://github.com/gabime/spdlog)
- [stb](https://github.com/nothings/stb)
- [tclap](https://github.com/mirror/tclap)
- [unordered_dense](https://github.com/martinus/unordered_dense)
- [xxHash](https://github.com/Cyan4973/xxHash)
