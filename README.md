# Paralela-proj1: sCATsaver

Made with <3 using SDL, ImGui, and OpenMP.

## Requirements

- **Operating system**: Linux, WSL (Ubuntu 20.04+), or macOS
- **Compiler**: GCC ≥ 9 or Clang ≥ 10
- **CMake**: ≥ 3.10 (3.13+ recommended)
- **System dependencies**: SDL, OpenMP

```bash
sudo apt install build-essential cmake pkg-config \
libglfw3-dev libglew-dev libglu1-mesa-dev mesa-common-dev \
libx11-dev libxrandr-dev libxi-dev libxinerama-dev libxcursor-dev \
libsdl2-dev libsdl2-image-dev
```

## Building the project

```bash
git submodule add https://github.com/ocornut/imgui.git external/imgui
git submodule update --init --recursive

chmod +x compile.sh
./compile.sh
```

## References

https://processing.org/examples/flocking.html

https://ddavidhahn.github.io/cs184_projfinal/