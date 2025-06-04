#include <imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>

#include <filesystem>
#include <vector>

#include <thread>
#include <mutex>
#include <unordered_map>

namespace fs = std::filesystem;

ImVec2 g_window_size(1280, 720);

std::mutex g_mutex;
std::unordered_map<std::string, uintmax_t> g_dir_sizes;
std::unordered_map<std::string, bool> g_dir_calculating;


void glfw_error_callback(int error, const char* description) 
{
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

void glfw_window_size_callback(GLFWwindow* p_window, int width, int height)
{
    g_window_size.x = static_cast<float>(width);
    g_window_size.y = static_cast<float>(height);
}


uintmax_t get_directory_size(const fs::path& dir) {
    uintmax_t size = 0;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied)) {
            try {
                if (entry.is_regular_file()) {
                    size += entry.file_size();
                }
            }
            catch (...) {
                // ignore skip_permission denied
            }
        }
    }
    catch (...) {
        // ignore skip_permission denied
    }
    return size;
}

void async_calculate_size(const fs::path& dir) {
    try {
        uintmax_t size = get_directory_size(dir);

        std::lock_guard<std::mutex> lock(g_mutex);
        g_dir_sizes[dir.string()] = size;
        g_dir_calculating[dir.string()] = false;
    }
    catch (const std::exception& e) {
        std::cerr << "Error calculating size for " << dir << ": " << e.what() << std::endl;
        std::lock_guard<std::mutex> lock(g_mutex);
        g_dir_calculating[dir.string()] = false;
    }
}

std::string format_size(uintmax_t bytes) {
    const char* sizes[] = { "B", "KB", "MB", "GB", "TB"};
    int i = 0;
    double dblBytes = static_cast<double>(bytes);
    while (dblBytes >= 1024 && i < 4) {
        dblBytes /= 1024;
        ++i;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f %s", dblBytes, sizes[i]);
    return std::string(buf);
}

void render_directory(const fs::path& path) {
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            std::string name = entry.path().filename().string();
            std::string size_str;

            if (entry.is_directory()) {
                std::string entry_path_str = entry.path().string();

                bool calculating = false;
                uintmax_t size = 0;

                {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    calculating = g_dir_calculating[entry_path_str];
                    if (g_dir_sizes.count(entry_path_str)) {
                        size = g_dir_sizes[entry_path_str];
                    }
                }

                if (size == 0 && !calculating) {
                    {
                        std::lock_guard<std::mutex> lock(g_mutex);
                        g_dir_calculating[entry_path_str] = true;
                    }
                    std::thread(async_calculate_size, entry.path()).detach();
                }

                size_str = calculating ? "..." : format_size(size);

                if (ImGui::TreeNode((name + " (" + size_str + ")").c_str())) {
                    ImGui::Indent(20.0f);
                    render_directory(entry.path());
                    ImGui::Unindent(20.0f);
                    ImGui::TreePop();
                }
            }
            else if (entry.is_regular_file()) {
                uintmax_t file_size = entry.file_size();
                size_str = format_size(file_size);
                ImGui::Text("%s (%s)", name.c_str(), size_str.c_str());
            }
        }
    }
    catch (...)
    {
        // ignore skip_permission denied
    }
}

int main() try
{
#ifndef _DEBUG
    FreeConsole();
#endif // !_DEBUG

    if (!glfwInit()) {
        std::cerr << "glfwInit failed!" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* p_window = glfwCreateWindow(static_cast<int>(g_window_size.x), static_cast<int>(g_window_size.y), "FileLister", nullptr, nullptr);
    if (!p_window) {
        std::cerr << "glfwCreateWindow failed!" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwSetWindowSizeCallback(p_window, glfw_window_size_callback);
    glfwSetErrorCallback(glfw_error_callback);

    glfwMakeContextCurrent(p_window);
    glfwSwapInterval(1);

    if (!gladLoadGL()) {
        std::cerr << "Can't load GLAD!" << std::endl;
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::GetIO().IniFilename = nullptr;
    io.FontGlobalScale = 1.5f;

    ImGui::StyleColorsDark();


    const char* glsl_version = "#version 130";

    ImGui_ImplGlfw_InitForOpenGL(p_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    while (!glfwWindowShouldClose(p_window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos({ 0,0 });
        ImGui::SetNextWindowSize(g_window_size);



        ImGui::Begin(" ", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

        ImGui::SliderFloat("Scale", &io.FontGlobalScale, 1.f, 2.f);

        fs::path dir = fs::current_path();
        std::string current_path = dir.string();


        if (ImGui::TreeNode(current_path.c_str())) {
            
            render_directory(dir);

            ImGui::TreePop();
        }

        ImGui::End();


        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(p_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(p_window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(p_window);
    glfwTerminate();

    return 0;
}
catch (std::exception e)
{
    std::cerr << e.what() << std::endl;
}
