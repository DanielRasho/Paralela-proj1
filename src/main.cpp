#include <iostream>
#include <sstream>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#ifdef _OPENMP
  #include <omp.h>
#else
  static int omp_get_max_threads() { return 1; }
  static int omp_get_num_threads() { return 1; }
#endif

struct Options {
    int width  = 0; // si 0, se solicitará por consola
    int height = 0; // si 0, se solicitará por consola
};

static bool parseInt(const char* s, int& out) {
    if (!s) return false;
    std::istringstream iss(s);
    iss >> out;
    return !iss.fail();
}

static Options parseArgs(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto eat = [&](const std::string& key){
            if (a == key && i + 1 < argc) return std::string(argv[++i]);
            if (a.rfind(key + "=", 0) == 0) return a.substr(key.size() + 1);
            return std::string();
        };
        if (auto v = eat("--width");  !v.empty())  parseInt(v.c_str(), opt.width);
        else if (auto v = eat("--height"); !v.empty()) parseInt(v.c_str(), opt.height);
        else if (a == "-w" && i + 1 < argc) parseInt(argv[++i], opt.width);
        else if (a == "-h" && i + 1 < argc) parseInt(argv[++i], opt.height);
        else if (a == "-?" || a == "--help") {
            std::cout << "Uso: screensaver [--width W] [--height H]\n";
            std::exit(0);
        }
    }
    return opt;
}

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

static void framebuffer_size_callback(GLFWwindow* /*window*/, int w, int h) {
    glViewport(0, 0, w, h);
}

int main(int argc, char** argv) {
    Options opt = parseArgs(argc, argv);

    if (opt.width <= 0)  opt.width  = askInt("Ancho de la ventana", 640, 800);
    if (opt.height <= 0) opt.height = askInt("Alto de la ventana",  480, 600);

    if (opt.width  < 640) opt.width  = 640;
    if (opt.height < 480) opt.height = 480;

    if (!glfwInit()) {
        std::cerr << "[Error] glfwInit()\n";
        return 1;
    }

    // Contexto OpenGL 3.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(opt.width, opt.height, "Screensaver", nullptr, nullptr);
    if (!window) {
        std::cerr << "[Error] glfwCreateWindow()\n";
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "[Error] gladLoadGLLoader()\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    int fbw, fbh; glfwGetFramebufferSize(window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Verificación mínima de OpenMP
    int max_threads = 1;
    int used_threads = 1;
#ifdef _OPENMP
    max_threads = omp_get_max_threads();
    #pragma omp parallel
    {
        #pragma omp single
        used_threads = omp_get_num_threads();
    }
#endif

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, 1);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Setup OK — Screensaver (OpenGL)");
        ImGui::Text("Ventana: %dx%d", opt.width, opt.height);
#ifdef _OPENMP
        ImGui::Text("OpenMP activo: max=%d, usados=%d", max_threads, used_threads);
#else
        ImGui::Text("OpenMP no detectado en la compilación.");
#endif
        ImGui::Separator();
        ImGui::Text("Presiona ESC o cierra la ventana para salir.");
        ImGui::End();

        ImGui::Render();
        glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}