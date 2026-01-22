#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <ctime>
#include <algorithm>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <dwmapi.h>
#include <windows.h>
#pragma comment(lib, "dwmapi.lib")

// Store original window procedure
WNDPROC g_OriginalWndProc = nullptr;
bool g_InTitleBar = false;

// Custom window procedure for smooth dragging
LRESULT CALLBACK CustomWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_NCHITTEST: {
        LRESULT hit = CallWindowProc(g_OriginalWndProc, hwnd, uMsg, wParam, lParam);
        if (hit == HTCLIENT && g_InTitleBar) {
            return HTCAPTION;  // Treat as title bar for smooth dragging
        }
        return hit;
    }
    }
    return CallWindowProc(g_OriginalWndProc, hwnd, uMsg, wParam, lParam);
}
#endif

struct TodoItem {
    std::string text;
    bool completed;
    time_t createdAt;

    TodoItem(const std::string& t) : text(t), completed(false) {
        createdAt = time(nullptr);
    }

    TodoItem(const std::string& t, bool c, time_t ct) : text(t), completed(c), createdAt(ct) {}
};

// Auto-save functions
void saveTodos(const std::vector<TodoItem>& todos) {
    std::ofstream file("todos.dat");
    if (file.is_open()) {
        for (const auto& todo : todos) {
            file << todo.completed << "|" << todo.createdAt << "|" << todo.text << "\n";
        }
        file.close();
    }
}

void loadTodos(std::vector<TodoItem>& todos) {
    std::ifstream file("todos.dat");
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            size_t firstPipe = line.find('|');
            size_t secondPipe = line.find('|', firstPipe + 1);

            if (firstPipe != std::string::npos && secondPipe != std::string::npos) {
                bool completed = (line[0] == '1');
                time_t createdAt = std::stoll(line.substr(firstPipe + 1, secondPipe - firstPipe - 1));
                std::string text = line.substr(secondPipe + 1);

                todos.emplace_back(text, completed, createdAt);
            }
        }
        file.close();
    }
}

int main(int, char**) {
#if defined(_WIN32)
    SetProcessDPIAware();
#endif

    if (!glfwInit())
        return -1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    // Make window transparent and frameless
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);  // Always on top

    GLFWwindow* window = glfwCreateWindow(400, 400, "To-Do List", NULL, NULL);
    if (window == NULL)
        return -1;

#if defined(_WIN32)
    HWND hwnd = glfwGetWin32Window(window);

    // Subclass the window for smooth dragging
    g_OriginalWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)CustomWndProc);

    // Make window layered for transparency support
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);

    // Set window to be topmost
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // Enable transparency with DWM
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
#endif

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    float xscale, yscale;
    glfwGetWindowContentScale(window, &xscale, &yscale);

    ImFontConfig font_cfg;
    font_cfg.OversampleH = 2;
    font_cfg.OversampleV = 2;
    font_cfg.SizePixels = 13.0f * xscale;
    io.Fonts->AddFontDefault(&font_cfg);

    ImGui::StyleColorsDark();

    // Customize style for overlay appearance
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(xscale);
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.85f);  // Semi-transparent
    style.Colors[ImGuiCol_Border] = ImVec4(0.4f, 0.8f, 1.0f, 0.5f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    std::vector<TodoItem> todos;
    char inputBuffer[256] = "";
    bool showCompleted = true;
    bool needsSave = false;

    // Load todos on startup
    loadTodos(todos);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);

        ImGui::Begin("To-Do List", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

        // Custom title bar for dragging
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 0.9f));

        ImGui::Button("##titlebar", ImVec2(ImGui::GetWindowWidth() - 55, 30));

        // Detect if mouse is in title bar for smooth dragging
#if defined(_WIN32)
        g_InTitleBar = ImGui::IsItemHovered();
#endif

        ImGui::SameLine(20);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "My To Do List");

        ImGui::SameLine(ImGui::GetWindowWidth() - 35);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
        if (ImGui::Button("X", ImVec2(25, 25))) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        ImGui::PopStyleColor(3);

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Add new task:");
        ImGui::SetNextItemWidth(-80);
        bool enterPressed = ImGui::InputText("##input", inputBuffer, IM_ARRAYSIZE(inputBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::SameLine();
        if ((ImGui::Button("Add", ImVec2(70, 0)) || enterPressed) && inputBuffer[0] != '\0') {
            todos.emplace_back(inputBuffer);
            inputBuffer[0] = '\0';
            needsSave = true;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        int totalTasks = (int)todos.size();
        int completedTasks = 0;
        for (const auto& todo : todos) if (todo.completed) completedTasks++;

        ImGui::Text("Tasks: %d total | %d completed", totalTasks, completedTasks);
        ImGui::SameLine(ImGui::GetWindowWidth() - 115);
        ImGui::Checkbox("Show Done", &showCompleted);

        ImGui::BeginChild("TaskList", ImVec2(0, -32.5), true);
        for (size_t i = 0; i < todos.size(); i++) {
            if (!showCompleted && todos[i].completed) continue;

            ImGui::PushID((int)i);
            bool prevCompleted = todos[i].completed;
            ImGui::Checkbox("##check", &todos[i].completed);
            if (prevCompleted != todos[i].completed) {
                needsSave = true;
            }
            ImGui::SameLine();

            if (todos[i].completed) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                ImGui::TextWrapped("%s", todos[i].text.c_str());
                ImGui::PopStyleColor();
            }
            else {
                ImGui::TextWrapped("%s", todos[i].text.c_str());
            }

            ImGui::SameLine(ImGui::GetWindowWidth() - 70);
            if (ImGui::Button("Delete")) {
                todos.erase(todos.begin() + i);
                needsSave = true;
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::Spacing();

        if (ImGui::Button("Clear Completed")) {
            size_t prevSize = todos.size();
            todos.erase(std::remove_if(todos.begin(), todos.end(),
                [](const TodoItem& t) { return t.completed; }), todos.end());
            if (todos.size() != prevSize) {
                needsSave = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear All")) {
            if (!todos.empty()) {
                todos.clear();
                needsSave = true;
            }
        }

        ImGui::End();

        // Auto-save if changes were made
        if (needsSave) {
            saveTodos(todos);
            needsSave = false;
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);  // Transparent background
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
