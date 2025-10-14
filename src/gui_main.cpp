#include <string>
#include <iostream>
#include <vector>
#include <atomic>
#include <thread>
#include <filesystem>
#include <algorithm>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX  // Prevent Windows from defining min/max macros
    #include <windows.h>
    #include <d3d11.h>
    #include <tchar.h>
    #pragma comment(lib, "d3d11.lib")
    #define PLATFORM_WINDOWS
#else
    #include <glad/glad.h>
    #include <GLFW/glfw3.h>
    #define PLATFORM_LINUX
#endif

#include "imgui.h"
#ifdef PLATFORM_WINDOWS
    #include "backends/imgui_impl_win32.h"
    #include "backends/imgui_impl_dx11.h"
#else
    #include "backends/imgui_impl_glfw.h"
    #include "backends/imgui_impl_opengl3.h"
#endif

#include "tinyfiledialogs.h"
#include "walkk.h"

#ifdef PLATFORM_WINDOWS
// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// DirectX11 data
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#else
static void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}
#endif


void SetupImGuiStyle()
{
	// ayu-dark style by usrnatc from ImThemes
	ImGuiStyle& style = ImGui::GetStyle();
	
	style.Alpha = 1.0f;
	style.DisabledAlpha = 0.6f;
	style.WindowPadding = ImVec2(8.0f, 8.0f);
	style.WindowRounding = 5.0f;
	style.WindowBorderSize = 1.0f;
	style.WindowMinSize = ImVec2(32.0f, 32.0f);
	style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
	style.WindowMenuButtonPosition = ImGuiDir_Left;
	style.ChildRounding = 0.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupRounding = 0.0f;
	style.PopupBorderSize = 1.0f;
	style.FramePadding = ImVec2(4.0f, 3.0f);
	style.FrameRounding = 5.0f;
	style.FrameBorderSize = 0.0f;
	style.ItemSpacing = ImVec2(8.0f, 4.0f);
	style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
	style.CellPadding = ImVec2(4.0f, 2.0f);
	style.IndentSpacing = 20.0f;
	style.ColumnsMinSpacing = 6.0f;
	style.ScrollbarSize = 12.9f;
	style.ScrollbarRounding = 9.0f;
	style.GrabMinSize = 8.0f;
	style.GrabRounding = 5.0f;
	style.TabRounding = 4.0f;
	style.TabBorderSize = 1.0f;
	style.TabMinWidthForCloseButton = 0.0f;
	style.ColorButtonPosition = ImGuiDir_Right;
	style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
	style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
	
	style.Colors[ImGuiCol_Text] = ImVec4(0.9019608f, 0.7058824f, 0.3137255f, 1.0f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.9019608f, 0.7058824f, 0.3137255f, 0.5019608f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.039215688f, 0.05490196f, 0.078431375f, 1.0f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.078431375f, 0.078431375f, 0.078431375f, 0.94f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.42745098f, 0.42745098f, 0.49803922f, 0.5f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.07450981f, 0.09019608f, 0.12941177f, 1.0f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.2509804f, 0.25882354f, 0.2784314f, 1.0f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.2509804f, 0.25882354f, 0.2784314f, 1.0f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5019608f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.05882353f, 0.07450981f, 0.101960786f, 1.0f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.0f, 0.0f, 0.5019608f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.043137256f, 0.05490196f, 0.078431375f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.019607844f, 0.019607844f, 0.019607844f, 0.53f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30980393f, 0.30980393f, 0.30980393f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40784314f, 0.40784314f, 0.40784314f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.50980395f, 0.50980395f, 0.50980395f, 1.0f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.24705882f, 0.69803923f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.9019608f, 0.7058824f, 0.3137255f, 1.0f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.56078434f, 0.2509804f, 1.0f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.2509804f, 0.25882354f, 0.2784314f, 1.0f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.30980393f, 0.31764707f, 0.3372549f, 1.0f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.2509804f, 0.25882354f, 0.2784314f, 1.0f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.2509804f, 0.25882354f, 0.2784314f, 1.0f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.30980393f, 0.31764707f, 0.3372549f, 1.0f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.30980393f, 0.31764707f, 0.3372549f, 1.0f);
	style.Colors[ImGuiCol_Separator] = ImVec4(0.2509804f, 0.25882354f, 0.2784314f, 1.0f);
	style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.30980393f, 0.31764707f, 0.3372549f, 1.0f);
	style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.2509804f, 0.25882354f, 0.2784314f, 1.0f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.24705882f, 0.69803923f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.13333334f, 0.4117647f, 0.54901963f, 1.0f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.13333334f, 0.4117647f, 0.54901963f, 1.0f);
	style.Colors[ImGuiCol_Tab] = ImVec4(0.07450981f, 0.09019608f, 0.12941177f, 1.0f);
	style.Colors[ImGuiCol_TabHovered] = ImVec4(0.2509804f, 0.25882354f, 0.2784314f, 1.0f);
	style.Colors[ImGuiCol_TabActive] = ImVec4(0.2509804f, 0.25882354f, 0.2784314f, 1.0f);
	style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.06666667f, 0.101960786f, 0.14509805f, 0.9724f);
	style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.13333334f, 0.25882354f, 0.42352942f, 1.0f);
	style.Colors[ImGuiCol_PlotLines] = ImVec4(0.60784316f, 0.60784316f, 0.60784316f, 1.0f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.24705882f, 0.69803923f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.24705882f, 0.69803923f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.13333334f, 0.4117647f, 0.54901963f, 1.0f);
	style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.2509804f, 0.25882354f, 0.2784314f, 1.0f);
	style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.2509804f, 0.25882354f, 0.2784314f, 1.0f);
	style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.039215688f, 0.05490196f, 0.078431375f, 1.0f);
	style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.039215688f, 0.05490196f, 0.078431375f, 1.0f);
	style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.06666667f, 0.10980392f, 0.16078432f, 1.0f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.30980393f, 0.30980393f, 0.34901962f, 1.0f);
	style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.24705882f, 0.69803923f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.9764706f, 0.25882354f, 0.25882354f, 1.0f);
	style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
	style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.8f, 0.8f, 0.8f, 0.2f);
	style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.8f, 0.8f, 0.8f, 0.35f);
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

#ifdef PLATFORM_WINDOWS
    // Windows DirectX11 initialization
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"walkk", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"walkk GUI", WS_OVERLAPPEDWINDOW, 100, 100, 960, 600, nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    SetupImGuiStyle();
    ImGui::GetStyle().ScaleAllSizes(1.0f);
    io.FontGlobalScale = 1.0f;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

#else
    // Linux GLFW+OpenGL initialization
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow* window = glfwCreateWindow(960, 600, "walkk GUI", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    SetupImGuiStyle();
    ImGui::GetStyle().ScaleAllSizes(1.0f);
    io.FontGlobalScale = 1.0f;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
#endif

    const int kSinkRate = 48000;
    const int kSinkChannels = 2;
    const size_t sinkCapacity = (size_t)kSinkRate * (size_t)kSinkChannels * 2;
    Walkk walkk(sinkCapacity);

    bool recursive = false;
    bool loaded = false;
    std::string directoryPath;

    CallbackData callbackData{ &walkk.sink, kSinkChannels };
    PaStream* stream = nullptr;
    std::thread producer;
    std::thread loader;
    bool playing = false;
    bool loading = false;
    int loadResult = -1;

    bool done = false;
    while (!done) {
#ifdef PLATFORM_WINDOWS
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
#else
        if (glfwWindowShouldClose(window)) break;
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
#endif
        ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
        ImGui::Begin("walkk", nullptr, window_flags);

        static const char* kAsciiArt = R"(.::    .   .::::::.      :::      :::  .   :::  .   
';;,  ;;  ;;;' ;;`;;     ;;;      ;;; .;;,.;;; .;;,.
 '[[, [[, [[' ,[[ '[[,   [[[      [[[[[/'  [[[[[/'  
   Y$c$$$c$P c$$$cc$$$c  $$'     _$$$$,   _$$$$,    
    "88"888   888   888,o88oo,.__"888"88o,"888"88o, 
     "M "M"   YMM   ""` """"YUMMM MMM "MMP"MMM "MMP" by gectheory)";

        ImGui::TextUnformatted(kAsciiArt);
        ImGui::Text("folder of mp3s...");

        static char dirBuf[1024] = {0};
        if (directoryPath.size() >= sizeof(dirBuf)) directoryPath.resize(sizeof(dirBuf)-1);
        std::snprintf(dirBuf, sizeof(dirBuf), "%s", directoryPath.c_str());
        if (ImGui::InputText("Directory", dirBuf, sizeof(dirBuf))) {
            directoryPath = dirBuf;
        }
        ImGui::SameLine();
        ImGui::Checkbox("Recursive", &recursive);

        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            const char* selectedPath = tinyfd_selectFolderDialog("Select Folder", directoryPath.empty() ? nullptr : directoryPath.c_str());
            if (selectedPath) {
                directoryPath = selectedPath;
            }
        }

        if (!playing) {
            if (!loading) {
                if (ImGui::Button("Load & Play")) {
                    if (loader.joinable()) loader.join();
                    walkk.files.clear();
                    loading = true;
                    loadResult = -1;
                    loader = std::thread([&walkk, &loading, &loadResult, directoryPath, recursive]() {
                        int res = 1;
                        if (!directoryPath.empty()) {
                            res = loadDirectoryMp3s(directoryPath.c_str(), walkk, recursive);
                        }
                        loadResult = res;
                        loading = false;
                    });
                }
            } else {
                ImGui::BeginDisabled();
                ImGui::Button("Loading...");
                ImGui::EndDisabled();
            }
        } else {
            if (ImGui::Button("Stop")) {
                walkk.allFinished.store(true);
                if (producer.joinable()) producer.join();
                if (stream) {
                    stopAndCloseStream(stream);
                    stream = nullptr;
                }
                playing = false;
            }
        }

        ImGui::Separator();
        size_t tried = 0, loadedCount = 0;
        {
            std::lock_guard<std::mutex> lk(walkk.loadStatsMutex);
            tried = walkk.filesAttemptedLastLoad;
            loadedCount = walkk.filesLoadedLast;
        }
        if (loading) {
            ImGui::Text("Loading... Tried: %zu  Loaded: %zu", tried, loadedCount);
        } else {
            ImGui::Text("Tried: %zu  Loaded: %zu  In set: %zu", tried, loadedCount, walkk.files.size());
        }

        if (!playing && !loading && loadResult == 0 && !walkk.files.empty()) {
            int err = openAndStartStream(&stream, &callbackData, kSinkChannels, kSinkRate, 256);
            if (err == paNoError) {
                playing = true;
                walkk.allFinished.store(false);
                if (producer.joinable()) producer.join();
                producer = std::thread([&walkk]() { granulizerLoop(&walkk); });
            }
            loadResult = -1;
        }

        {
            std::lock_guard<std::mutex> g(walkk.lastGrainMutex);
            if (!walkk.files.empty()) {
                if (walkk.lastGrain.hasExpectedStart) {
                    auto now = std::chrono::steady_clock::now();
                    if (now >= walkk.lastGrain.expectedStartTime && !walkk.lastGrain.hasStarted) {
                        walkk.lastGrain.hasStarted = true;
                        walkk.currentGrain = walkk.lastGrain;
                    }
                    if (!walkk.currentGrain.relPath.empty() && walkk.currentGrain.hasExpectedStart) {
                        if (now >= walkk.currentGrain.expectedEndTime) {
                            walkk.currentGrain = Walkk::GrainDebugInfo{};
                        }
                    }
                }
                std::string displayName = walkk.lastGrain.relPath;
                if (displayName.empty()) {
                    size_t idx = walkk.lastGrain.fileIndex;
                    if (idx < walkk.files.size()) {
                        const auto &sf = walkk.files[idx];
                        displayName = sf.relPath.empty() ? sf.path : sf.relPath;
                    }
                }
                if (!walkk.currentGrain.relPath.empty()) {
                    ImGui::Text("Now: %s", walkk.currentGrain.relPath.c_str());
                }
                if (!displayName.empty()) {
                    auto now = std::chrono::steady_clock::now();
                    if (walkk.lastGrain.hasExpectedStart && now < walkk.lastGrain.expectedStartTime) {
                        auto msLeft = std::chrono::duration_cast<std::chrono::milliseconds>(
                            walkk.lastGrain.expectedStartTime - now).count();
                        if (msLeft < 0) msLeft = 0;
                        ImGui::Text("Next: %s in %lld ms", displayName.c_str(), (long long)msLeft);
                    } else if (!walkk.lastGrain.hasStarted) {
                        ImGui::Text("Next: %s", displayName.c_str());
                    }
                }
                ImGui::Text("Grain: start=%zu frames  dur=%zu frames  amp=%.2f",
                    walkk.lastGrain.startFrame,
                    walkk.lastGrain.durationFrames,
                    walkk.lastGrain.amplitude);
                ImGui::Text("Loop: %s  win=%zu fr  drag=%d fr",
                    walkk.lastGrain.loopEnabled ? "on" : "off",
                    walkk.lastGrain.loopWindowFrames,
                    walkk.lastGrain.loopDragFrames);
            }
        }

        ImGui::Separator();
        ImGui::Text("History");
        ImGui::BeginChild("log", ImVec2(0, 200), true, ImGuiWindowFlags_HorizontalScrollbar);
        {
            std::lock_guard<std::mutex> lg(walkk.logMutex);
            for (const auto &line : walkk.logLines) {
                ImGui::TextUnformatted(line.c_str());
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();

        {
            std::lock_guard<std::mutex> lock(walkk.settingsMutex);
            ImGui::Text("Granular Settings");
            int minGrain = (int)walkk.settings.minGrainMs;
            int maxGrain = (int)walkk.settings.maxGrainMs;
            int overlap  = (int)walkk.settings.grainOverlapMs;
            int maxConc  = (int)walkk.settings.maxConcurrentGrains;
            float loopProb = walkk.settings.loopProbability;
            int minWin = (int)walkk.settings.minLoopWindowMs;
            int maxWin = (int)walkk.settings.maxLoopWindowMs;
            int maxDrag = (int)walkk.settings.maxLoopDragMs;
            int whiteNoise = (int)walkk.settings.whiteNoiseMs;
            float whiteNoiseVol = walkk.settings.whiteNoiseAmplitude;

            if (ImGui::SliderInt("Min Grain (ms)", &minGrain, 5, 5000)) {
                walkk.settings.minGrainMs = (size_t)std::max(1, minGrain);
                if (walkk.settings.minGrainMs > walkk.settings.maxGrainMs)
                    walkk.settings.maxGrainMs = walkk.settings.minGrainMs;
            }
            if (ImGui::SliderInt("Max Grain (ms)", &maxGrain, 5, 8000)) {
                walkk.settings.maxGrainMs = (size_t)std::max(1, maxGrain);
                if (walkk.settings.maxGrainMs < walkk.settings.minGrainMs)
                    walkk.settings.minGrainMs = walkk.settings.maxGrainMs;
            }
            ImGui::SliderInt("Overlap (ms)", &overlap, 0, 500);
            walkk.settings.grainOverlapMs = (size_t)std::max(0, overlap);

            ImGui::SliderInt("Max Concurrent Grains", &maxConc, 1, 16);
            walkk.settings.maxConcurrentGrains = (size_t)std::max(1, maxConc);

            ImGui::SliderFloat("Loop Probability", &loopProb, 0.0f, 1.0f);
            if (loopProb < 0.0f) loopProb = 0.0f; else if (loopProb > 1.0f) loopProb = 1.0f;
            walkk.settings.loopProbability = loopProb;

            if (ImGui::SliderInt("Min Loop Window (ms)", &minWin, 1, 5000)) {
                walkk.settings.minLoopWindowMs = (size_t)std::max(1, minWin);
                if (walkk.settings.minLoopWindowMs > walkk.settings.maxLoopWindowMs)
                    walkk.settings.maxLoopWindowMs = walkk.settings.minLoopWindowMs;
            }
            if (ImGui::SliderInt("Max Loop Window (ms)", &maxWin, 1, 5000)) {
                walkk.settings.maxLoopWindowMs = (size_t)std::max(1, maxWin);
                if (walkk.settings.maxLoopWindowMs < walkk.settings.minLoopWindowMs)
                    walkk.settings.minLoopWindowMs = walkk.settings.maxLoopWindowMs;
            }
            ImGui::SliderInt("Max Loop Drag (±ms)", &maxDrag, 0, 500);
            walkk.settings.maxLoopDragMs = std::max(0, maxDrag);

            ImGui::SliderInt("White Noise Duration (ms)", &whiteNoise, 0, 5000);
            if (whiteNoise < 0) whiteNoise = 0; if (whiteNoise > 5000) whiteNoise = 5000;
            walkk.settings.whiteNoiseMs = (size_t)whiteNoise;

            ImGui::SliderFloat("White Noise Volume", &whiteNoiseVol, 0.0f, 1.0f);
            if (whiteNoiseVol < 0.0f) whiteNoiseVol = 0.0f; if (whiteNoiseVol > 1.0f) whiteNoiseVol = 1.0f;
            walkk.settings.whiteNoiseAmplitude = whiteNoiseVol;
        }

        ImGui::PopStyleVar(3);
        ImGui::End();

        ImGui::Render();

#ifdef PLATFORM_WINDOWS
        const float clear_color_with_alpha[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
#else
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
#endif
    }

    walkk.allFinished.store(true);
    if (producer.joinable()) producer.join();
    if (loader.joinable()) loader.join();
    if (stream) stopAndCloseStream(stream);

#ifdef PLATFORM_WINDOWS
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
#else
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
#endif

    return 0;
}

#ifdef PLATFORM_WINDOWS
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
#endif