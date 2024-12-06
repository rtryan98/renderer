# Renderer
Research / Toy Renderer

## Table of Contents
- [Building](#building)
    - [Requirements](#requirements)
- [Features](#features)

## Building
Building Renderer requires the following:
- Windows 11
- Python 3.11
- CMake 3.28

The steps for Visual Studio 2022 are:
1. Clone the repository: `git clone https://github.com/rtryan98/renderer.git --recurse-submodules --shallow-submodules`.
2. Call CMake inside the cloned repository: `cmake -B build -G "Visual Studio 17 2022"`.
3. Open the solution file: `cmake --open build`.
4. Build the project inside Visual Studio.

Windows 10 and older Visual Studio versions are not tested and may not work.

## Features
- Inverse Fast Fourier Transform based Ocean Simulation
    - Utilizes oceanographic spectra presented in C. Horvath's 'Empirical directional wave spectra for computer graphics' [\[DOI\]](https://dl.acm.org/doi/10.1145/2791261.2791267).
    - The synthesized spectra are packed together to halve the amount of IFFTs required and thus double the performance of the simulation.
