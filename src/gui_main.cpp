#include <string>
#include <iostream>
#include <vector>
#include <atomic>
#include <thread>
#include <filesystem>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "walkk.h"

static void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

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
    // Increase overall UI scale a bit
    ImGui::GetStyle().ScaleAllSizes(1.25f);
    io.FontGlobalScale = 1.25f;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

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
    int loadResult = -1; // 0 success, non-zero fail

    // Simple cross-platform (ImGui-based) folder browser state
    bool openFolderPopup = false;
    std::filesystem::path browsePath;
    std::string browsePathBuf;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Make main window fill the entire viewport (no decorations or padding)
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
        ImGui::Begin("walkk", nullptr, window_flags);

        // C++11+
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
            try {
                if (!directoryPath.empty() && std::filesystem::is_directory(directoryPath)) {
                    browsePath = std::filesystem::path(directoryPath);
                } else {
                    // Try HOME or USERPROFILE, otherwise current_path
                    const char* home = std::getenv("HOME");
                    const char* userprofile = std::getenv("USERPROFILE");
                    if (home && std::filesystem::is_directory(home)) {
                        browsePath = std::filesystem::path(home);
                    } else if (userprofile && std::filesystem::is_directory(userprofile)) {
                        browsePath = std::filesystem::path(userprofile);
                    } else {
                        browsePath = std::filesystem::current_path();
                    }
                }
            } catch (...) {
                browsePath = std::filesystem::current_path();
            }
            browsePathBuf = browsePath.string();
            openFolderPopup = true;
            ImGui::OpenPopup("Select Folder");
        }

        // Single toggle button: Load & Play (async load)
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
        // Loading/queue indicators
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

        // Auto-start playback when loading completed successfully
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

        // Currently playing grain/file indicators with ETA-aware display
        {
            std::lock_guard<std::mutex> g(walkk.lastGrainMutex);
            if (!walkk.files.empty()) {
                // Promote to current when ETA passes
                if (walkk.lastGrain.hasExpectedStart) {
                    auto now = std::chrono::steady_clock::now();
                    if (now >= walkk.lastGrain.expectedStartTime && !walkk.lastGrain.hasStarted) {
                        walkk.lastGrain.hasStarted = true;
                        walkk.currentGrain = walkk.lastGrain;
                    }
                    // Clear current when its expected end has passed to avoid "stuck" Now
                    if (!walkk.currentGrain.relPath.empty() && walkk.currentGrain.hasExpectedStart) {
                        if (now >= walkk.currentGrain.expectedEndTime) {
                            walkk.currentGrain = Walkk::GrainDebugInfo{};
                        }
                    }
                }
                std::string displayName = walkk.lastGrain.relPath;
                if (displayName.empty()) {
                    // Try to compute a basename from current file index
                    size_t idx = walkk.lastGrain.fileIndex;
                    if (idx < walkk.files.size()) {
                        const auto &sf = walkk.files[idx];
                        displayName = sf.relPath.empty() ? sf.path : sf.relPath;
                    }
                }
                // Current and Next labels: use currentGrain for Now, lastGrain for Next
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

        // Folder selection modal (ImGui-based, works on Win/Linux/macOS)
        if (openFolderPopup) {
            if (ImGui::BeginPopupModal("Select Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Current: %s", browsePath.string().c_str());
                if (ImGui::Button("Up")) {
                    if (browsePath.has_parent_path()) browsePath = browsePath.parent_path();
                }
                ImGui::SameLine();
                if (ImGui::Button("Select")) {
                    directoryPath = browsePath.string();
                    ImGui::CloseCurrentPopup();
                    openFolderPopup = false;
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                    openFolderPopup = false;
                }

                ImGui::BeginChild("folder_list", ImVec2(700, 400), true);
                try {
                    for (const auto &entry : std::filesystem::directory_iterator(browsePath)) {
                        std::error_code ec;
                        bool isDir = entry.is_directory(ec);
                        if (!ec && isDir) {
                            std::string name = entry.path().filename().string();
                            if (ImGui::Selectable(name.c_str(), false)) {
                                browsePath = entry.path();
                            }
                        }
                    }
                } catch (...) {
                    // ignore errors
                }
                ImGui::EndChild();

                ImGui::EndPopup();
            }
        }

        ImGui::Separator();
        // Console-like log history
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

        // Live granular settings (protected by mutex)
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

        // Play/Stop handled above together with loading

        ImGui::PopStyleVar(3);
        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Cleanup
    walkk.allFinished.store(true);
    if (producer.joinable()) producer.join();
    if (loader.joinable()) loader.join();
    if (stream) stopAndCloseStream(stream);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}


