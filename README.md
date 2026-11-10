# MandelGL

MandelGL is a small OpenGL-based Mandelbrot explorer written in C++23. It renders the fractal entirely on the GPU using a compute shader and displays it on a full-screen quad.

## Features

- Real-time GPU Mandelbrot rendering via OpenGL compute shaders
- Smooth iterative color palette for visual rendering
- Keyboard-based panning with WASD and arrow keys
- Mouse-wheel zooming
- Simple CMake build system
- Debug OpenGL context support

## Requirements

To build this project, you need:

- CMake 3.10 or newer
- C++23 compatible compiler
- OpenGL 4.6 support
- GLEW
- GLFW
- GLM

On Ubuntu/Debian-based systems, install dependencies with:

```bash
sudo apt update
sudo apt install -y build-essential cmake libglew-dev libglfw3-dev libglm-dev
```

## Build

From the project root:

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

This produces the `mandel-main` executable.

## Run

```bash
./mandel-main
```

## Controls

- `W` / `A` / `S` / `D` or arrow keys: pan the fractal
- Mouse wheel: zoom in and out

## Project Structure

```text
MandelGL/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── resource-manager.hpp
│   └── utils.hpp
├── shaders/
│   ├── default.frag
│   ├── default.geom
│   ├── default.vert
│   └── mandel.comp
├── src/
│   ├── main.cpp
│   └── resource-manager.cpp
└── build/
```

## How it Works

The application creates a GLFW window, initializes GLEW, and sets up OpenGL resources. The Mandelbrot set is rendered into an image texture using the compute shader in `shaders/mandel.comp`, then displayed using a full-screen quad and the fragment shader in `shaders/default.frag`.

The render loop updates the fractal center and zoom as the user moves and zooms, providing a smooth interactive exploration experience.

## Notes

This project is intended as a lightweight learning/demo project for GPU fractal rendering and OpenGL compute pipelines.
