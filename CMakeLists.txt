cmake_minimum_required(VERSION 3.15)
project(screensaver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Evitar warning de OpenGL: prefiere GLVND en Linux
if(POLICY CMP0072)
  cmake_policy(SET CMP0072 NEW)
endif()

# OpenMP
find_package(OpenMP REQUIRED)

# OpenGL
find_package(OpenGL REQUIRED)

# GLFW – intenta primero paquete CONFIG (vcpkg/brew), si no, pkg-config
find_package(glfw3 QUIET CONFIG)
if(NOT glfw3_FOUND)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(GLFW3 REQUIRED glfw3)
    add_library(glfw3::glfw INTERFACE IMPORTED)
    target_include_directories(glfw3::glfw INTERFACE ${GLFW3_INCLUDE_DIRS})
    target_link_libraries(glfw3::glfw INTERFACE ${GLFW3_LIBRARIES})
  else()
    message(FATAL_ERROR "GLFW no encontrado. Instala glfw3 (dev) o usa vcpkg/brew.")
  endif()
endif()

# GLAD (loader de OpenGL) – via FetchContent
include(FetchContent)
FetchContent_Declare(
  glad
  GIT_REPOSITORY https://github.com/Dav1dde/glad.git
  GIT_TAG        v0.1.36  
)
FetchContent_MakeAvailable(glad)

# Dear ImGui (submódulo)
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
set(IMGUI_SOURCES
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_executable(screensaver
  src/main.cpp
  ${IMGUI_SOURCES}
)

target_include_directories(screensaver PRIVATE
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
)

# Definir el loader para el backend OpenGL3 de ImGui
# (usar GLAD que acabamos de traer)
if(MSVC)
  target_compile_definitions(screensaver PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
else()
  add_definitions(-DIMGUI_IMPL_OPENGL_LOADER_GLAD)
endif()

# Vincular dependencias
# GLFW (distintos targets según el origen)
if(TARGET glfw)
  target_link_libraries(screensaver PRIVATE glfw)
elseif(TARGET glfw3)
  target_link_libraries(screensaver PRIVATE glfw3)
elseif(TARGET glfw3::glfw)
  target_link_libraries(screensaver PRIVATE glfw3::glfw)
else()
  message(FATAL_ERROR "No se encontró un target de GLFW para enlazar.")
endif()

# OpenGL, GLAD y OpenMP
target_link_libraries(screensaver PRIVATE OpenGL::GL glad OpenMP::OpenMP_CXX)

# Warnings
if(MSVC)
  target_compile_options(screensaver PRIVATE /W4)
else()
  target_compile_options(screensaver PRIVATE -Wall -Wextra -Wpedantic)
endif()