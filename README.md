# Renderer
Research / Toy Renderer

## Table of Contents
- [Building](#building)
    - [Requirements](#requirements)
- [Features](#features)

## Building
Building Renderer requires the following:
- Windows 10
- Visual Studio 2022
- Python 3.0
- CMake 3.28

The steps are:
1. Clone the repository: `git clone https://github.com/rtryan98/renderer.git --recurse-submodules --shallow-submodules`.
2. Call CMake inside the cloned repository: `cmake -B build -G "Visual Studio 17 2022"`.

## Features
- Fast Fourier Transform based Ocean Simulation
    - Utilizes oceanographic spectra presented in C. Horvath's 'Empirical directional wave spectra for computer graphics' [\[DOI\]]([DOI](https://dl.acm.org/doi/10.1145/2791261.2791267)).
