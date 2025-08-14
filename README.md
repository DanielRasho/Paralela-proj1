# Paralela-proj1

## Requisitos

- **Sistema operativo:** Linux, WSL (Ubuntu 20.04+), o macOS
- **Compilador:** `GCC ≥ 9` o `Clang ≥ 10`
- **CMake:** `≥ 3.10` (se recomienda 3.13+)
- **Dependencias del sistema:**

```bash
sudo apt install build-essential cmake pkg-config \
libglfw3-dev libglew-dev libglu1-mesa-dev mesa-common-dev \
libx11-dev libxrandr-dev libxi-dev libxinerama-dev libxcursor-dev
```

## Construcción del Proyecto

```bash
git clone https://github.com/XavierLopez25/lit_locks_scheduler_ts.git
cd lit_locks_scheduler_ts

git submodule add https://github.com/ocornut/imgui.git external/imgui
git submodule update --init --recursive

chmod +x compile.sh
./compile.sh
```
