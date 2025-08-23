#include <cmath>
#include <string>
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <memory>
#include <algorithm>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"

// ===========================
//  CONSTANTS
// ==========================

constexpr float PI = 3.14159265359f;
constexpr float TWO_PI = 2.0f * PI;

// ===========================
//  STRUCTS
// ==========================


struct CLI_Options {
    int width = 0;
    int height = 0;
    int numBoids = 150;
    bool showStats = true;
    bool useParallel = true;
    bool showTrails = false;
};

// ===========================
//  UTILITY FUNCTION
// ==========================
static bool parseInt(const char* s, int& out) {
    if (!s) return false;
    std::istringstream iss(s);
    iss >> out;
    return !iss.fail();
}

// Representation of a 2D vector
// provides utility function to operate 
struct Vector2D {
    float x, y;

    // Constructors
    Vector2D() : x(0), y(0) {}
    Vector2D(float x_, float y_) : x(x_), y(y_) {}
    
    // Overload the arithmethic operators to 
    // let sintax like Vec1 + Vec2 instead of Vec1.x + Vec2.x & Vec1.y + Vec2.y
    
    Vector2D operator+(const Vector2D & other) const {
        return Vector2D(x + other.x, y + other.y);
    }

    Vector2D operator-(const Vector2D & other) const {
        return Vector2D(x - other.x, y - other.y);
    }

    Vector2D operator*(const Vector2D & other) const {
        return Vector2D(x * other.x, y * other.y);
    }

    Vector2D operator/(const Vector2D & other) const {
        return Vector2D(x / other.x, y / other.y);
    }

    void operator*=(float scalar) {
        x *= scalar;
        y *= scalar;
    }


    void operator/=(float scalar) {
        x /= scalar;
        y /= scalar;
    }
    
    // UTILITY FUNCTION
    
    float magnitude() const {
        return sqrt(x * x + y * y);
    }
    
    /*Scales a vector to have a magnitud of 1*/
    void normalize() {
        float mag = magnitude();
        if (mag > 0) {
            x /= mag;
            y /= mag;
        }
    }
    
    Vector2D normalized() const {
        Vector2D result = *this;
        result.normalized();
        return result;
    }
    
    /*Limits the scale of a vector to have at most maxMav magnitud*/
    void limit(float maxMag) {
        if (magnitude() > maxMag) {
            normalize();
            *this *= maxMag;
        }
    }
    
    float heading() const {
        return atan2(y, x);
    }

    // Return the distance between 2 scalars.
    static float distance(const Vector2D& a, const Vector2D& b) {
        return (a - b).magnitude();
    }
    
    // Returns an unitary vector from 
    static Vector2D fromAngle( float angle ) { 
        return Vector2D(cos(angle), sin(angle));
    }
};

// Retrieve passed on CLI
static CLI_Options parseArgs(int argc, char** argv) {
    CLI_Options opt;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        
        // Lambda to consume the next parameters
        auto eat = [&](const std::string& key) {
            if (a == key && i + i < argc) return std::string(argv[++i]);
            if (a.rfind(key + "=", 0) == 0) return a.substr(key.size() + 1);
            return std::string();
        };
        
        // Parses the first argument "n" number of birds
        int num;
        if (parseInt(a.c_str(), num) && num > 0) {
            opt.numBoids = num;
            continue;
        }

        // The rest of the arguments are passed with with flags
        if (auto v = eat("--width");  !v.empty())  parseInt(v.c_str(), opt.width);
        else if (auto v = eat("--height"); !v.empty()) parseInt(v.c_str(), opt.height);
        else if (auto v = eat("--boids"); !v.empty()) parseInt(v.c_str(), opt.numBoids);
        else if (a == "--no-gui") opt.showStats = false;
        else if (a == "--serial") opt.useParallel = false;
        else if (a == "--trails") opt.showTrails = true;
        // Print help message
        else if (a == "-?" || a == "--help") {
            std::cout << "Uso: flocking [num_boids] [opciones]\n";
            std::cout << "Opciones:\n";
            std::cout << "  --width W       Ancho de ventana\n";
            std::cout << "  --height H      Alto de ventana\n";
            std::cout << "  --boids B       Número de boids\n";
            std::cout << "  --no-gui        Sin overlay GUI\n";
            std::cout << "  --serial        Forzar modo serial (sin OpenMP)\n";
            std::cout << "  --trails        Mostrar estelas\n";
            std::cout << "Ejemplo: flocking 500 --width 1920 --height 1080 --trails\n";
            std::exit(0);
        }
    }

    return opt;
}

// ask for a number from std in, if not provided set fallback valu
static int askInt(const std::string& prompt, int minVal, int fallback) {
    while (true) {
        std::cout << prompt << " (min " << minVal << ", enter para " << fallback << "): ";
        std::string line; std::getline(std::cin, line);
        if (line.empty()) return fallback;
        std::istringstream iss(line);
        int v = 0; iss >> v;
        if (!iss.fail() && v >= minVal) return v;
        std::cout << "Valor inválido, intenta de nuevo.\n";
    }
}

// ==========================
// FLOCK SYSTEM
// ==========================

// ==========================
// MAIN
// ==========================
int main(int argc, char** argv) {

    CLI_Options opt = parseArgs(argc, argv);

    if (opt.width <= 0)  opt.width  = askInt("Ancho de la ventana", 640, 1280);
    if (opt.height <= 0) opt.height = askInt("Alto de la ventana",  480, 720);

    if (opt.width  < 640) opt.width  = 640;
    if (opt.height < 480) opt.height = 480;

    std::cout << "Iniciando simulación de flocking con " << opt.numBoids << " boids...\n";
    std::cout << "Modo: " << (opt.useParallel ? "Paralelo (OpenMP)" : "Serial") << "\n";
    if (opt.showTrails) std::cout << "Estelas activadas\n";

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "[Error] SDL_Init: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Flocking Birds Simulation",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        opt.width, opt.height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    
    if (!window) {
        std::cerr << "[Error] SDL_CreateWindow: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Instantiation of the renderer, controller that let us interact with sdl window.
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 
        SDL_RENDERER_SOFTWARE );

    if (!renderer) {
        std::cerr << "[Error] SDL_CreateRenderer: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Initialize ImGui if GUI is enabled
    if (opt.showStats) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        
        ImGui::StyleColorsDark();
        
        ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer2_Init(renderer);
    }
    
    // Performance tracking
    auto lastTime = std::chrono::high_resolution_clock::now();
    // auto lastFlockingTime = std::chrono::microseconds(0);
    auto lastUpdateTime = std::chrono::microseconds(0);
    auto lastRenderTime = std::chrono::microseconds(0);
    
    float fps = 0.0f;
    int frameCount = 0;
    auto lastStatsTime = lastTime;

    bool running = true;
    bool showDetailedStats = false;
    bool paused = false;
    
    // RENDER LOOP
    while (running) {

        auto frameStartTime = std::chrono::high_resolution_clock::now();

        // If there are unprocessed events, procces one at a time.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {

            // if show stats == true, render im gui stats window
            if (opt.showStats) {
                ImGui_ImplSDL2_ProcessEvent(&event);
            }
            
            if (event.type == SDL_QUIT){
                running = false;
            }
            
            else if (event.type == SDL_KEYDOWN) {
                
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                }
            }

            // Add boid at mouse position
            else if (event.type == SDL_MOUSEBUTTONDOWN) {
            }

            // Handle window events gracefully
            else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    opt.width = event.window.data1;
                    opt.height = event.window.data2;
                }
            }
        }

        // Render with timing
        auto renderStart = std::chrono::high_resolution_clock::now();
    
        SDL_SetRenderDrawColor(renderer, 20, 25, 40, 255);
        SDL_RenderClear(renderer);

        // Render ImGui overlay
        if (opt.showStats) {
            ImGui_ImplSDLRenderer2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            ImGui::SetNextWindowPos(ImVec2(10, 10));
            ImGui::SetNextWindowBgAlpha(0.8f);
            ImGui::Begin("Flocking", nullptr, 
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
            ImGui::Text("Boids: %zu | FPS: %.0f | %s", 
                       0, fps, 
                       opt.useParallel ? "PAR" : "SER");
            ImGui::Text("SPACE: pause | P: mode | T: trails | F1: stats");
            if (paused) ImGui::TextColored(ImVec4(1,1,0,1), "PAUSED");
            ImGui::End();

            ImGui::Render();
            // Flush im gui data to framebuffer.
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        }

        SDL_RenderPresent(renderer);
        
        auto renderEnd = std::chrono::high_resolution_clock::now();
        lastRenderTime = std::chrono::duration_cast<std::chrono::microseconds>(renderEnd - renderStart);

        // Calculate FPS
        frameCount++;
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto statsElapsed = std::chrono::duration<float>(currentTime - lastStatsTime).count();

        if (statsElapsed >= 1.0f) {
            fps = frameCount / statsElapsed;
            frameCount = 0;
            lastStatsTime = currentTime;
            
            // if (!opt.showStats) {
            //     std::cout << "\rBoids: " << flock.getBoidCount() 
            //              << " | FPS: " << static_cast<int>(fps)
            //              << " | Flocking: " << lastFlockingTime.count() << "μs"
            //              << " | " << (opt.useParallel ? "PAR" : "SER")
            //              << " | " << (paused ? "PAUSED" : "Running")
            //              << std::flush;
            // }
        }
        
        lastTime = frameStartTime;

    }

    // Resources cleanup
    if (opt.showStats) {
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    if (!opt.showStats) std::cout << "\n";
    std::cout << "Simulación terminada.\n";


    return 0;
}