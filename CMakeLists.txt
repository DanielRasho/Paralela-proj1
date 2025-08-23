cmake_minimum_required(VERSION 3.15)
project(screensaver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenMP
find_package(OpenMP REQUIRED)

# SDL2
find_package(SDL2 REQUIRED)
if(NOT SDL2_FOUND)
    # Fallback for different SDL2 package configurations
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 REQUIRED sdl2)
        pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
    else()
        message(FATAL_ERROR "SDL2 no encontrado. Instala libsdl2-dev y libsdl2-image-dev")
    endif()
endif()

# Try to find SDL2_image
find_package(SDL2_image QUIET)
if(NOT SDL2_image_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
    else()
        message(WARNING "SDL2_image no encontrado. Background image support disabled.")
    endif()
endif()

# Dear ImGui (submódulo o FetchContent)
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)

# Si no tienes imgui como submódulo, usa FetchContent
if(NOT EXISTS ${IMGUI_DIR}/imgui.cpp)
    include(FetchContent)
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        docking  # o usa 'master' para stable
    )
    FetchContent_MakeAvailable(imgui)
    set(IMGUI_DIR ${imgui_SOURCE_DIR})
endif()

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)

add_executable(screensaver
    src/main.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(screensaver PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# SDL2 linking - diferentes métodos según el sistema
if(TARGET SDL2::SDL2)
    # Nuevo estilo de SDL2 (Ubuntu 22.04+, vcpkg, etc.)
    target_link_libraries(screensaver PRIVATE SDL2::SDL2)
    if(TARGET SDL2_image::SDL2_image)
        target_link_libraries(screensaver PRIVATE SDL2_image::SDL2_image)
    endif()
elseif(SDL2_LIBRARIES)
    # Estilo tradicional con variables
    target_include_directories(screensaver PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(screensaver PRIVATE ${SDL2_LIBRARIES})
    if(SDL2_IMAGE_LIBRARIES)
        target_include_directories(screensaver PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
        target_link_libraries(screensaver PRIVATE ${SDL2_IMAGE_LIBRARIES})
    endif()
else()
    # Fallback manual
    find_library(SDL2_LIBRARY SDL2)
    find_library(SDL2_IMAGE_LIBRARY SDL2_image)
    target_link_libraries(screensaver PRIVATE ${SDL2_LIBRARY})
    if(SDL2_IMAGE_LIBRARY)
        target_link_libraries(screensaver PRIVATE ${SDL2_IMAGE_LIBRARY})
    endif()
endif()

# OpenMP
target_link_libraries(screensaver PRIVATE OpenMP::OpenMP_CXX)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(screensaver PRIVATE SDL2main)
elseif(APPLE)
    target_link_libraries(screensaver PRIVATE "-framework CoreFoundation")
elseif(UNIX)
    # Linux requires math library for some distributions
    target_link_libraries(screensaver PRIVATE m)
endif()

# Optimización para release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        target_compile_options(screensaver PRIVATE /O2 /DNDEBUG)
    else()
        target_compile_options(screensaver PRIVATE -O3 -DNDEBUG -march=native)
    endif()
endif()

# Warnings
if(MSVC)
    target_compile_options(screensaver PRIVATE /W4)
else()
    target_compile_options(screensaver PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Crear directorio para assets si no existe
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/assets)

# Copy example assets if they exist
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets/ DESTINATION ${CMAKE_BINARY_DIR}/assets/)
endif()

# Información de configuración
message(STATUS "=== Configuración del Screensaver ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "OpenMP: ${OpenMP_FOUND}")

if(TARGET SDL2::SDL2)
    message(STATUS "SDL2: Found (modern targets)")
elseif(SDL2_LIBRARIES)
    message(STATUS "SDL2: Found (${SDL2_LIBRARIES})")
else()
    message(STATUS "SDL2: Found (fallback)")
endif()

if(TARGET SDL2_image::SDL2_image OR SDL2_IMAGE_LIBRARIES)
    message(STATUS "SDL2_image: Found")
else()
    message(STATUS "SDL2_image: NOT FOUND (background support disabled)")
endif()

if(EXISTS ${IMGUI_DIR}/imgui.cpp)
    message(STATUS "ImGui: Found in ${IMGUI_DIR}")
else()
    message(STATUS "ImGui: Downloaded via FetchContent")
endif()

message(STATUS "")
message(STATUS "=== Uso ===")
message(STATUS "  ./screensaver 100                # 100 pelotas")
message(STATUS "  ./screensaver 50 --width 1920    # 50 pelotas, ventana específica")
message(STATUS "  ./screensaver 200 --no-gui       # Sin overlay, stats en terminal")
message(STATUS "  ./screensaver --help              # Ver todas las opciones")
message(STATUS "")
message(STATUS "=== Controles en tiempo real ===")
message(STATUS "  ESC: Salir")
message(STATUS "  F1:  Toggle estadísticas detalladas")
message(STATUS "  +/-: Agregar/quitar 10 pelotas")
message(STATUS "")
if(TARGET SDL2_image::SDL2_image OR SDL2_IMAGE_LIBRARIES)
    message(STATUS "Tip: Coloca cat.png en assets/ para fondo personalizado")
endif()