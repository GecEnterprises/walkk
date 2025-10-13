#include <string>
#include <iostream>
#include <vector>
#include <atomic>
#include <thread>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "walkk.h"

static void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
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
    ImGui::StyleColorsDark();

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
    bool playing = false;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("walkk");
        ImGui::Text("Load a folder of mp3s and play grains");

        static char dirBuf[1024] = {0};
        if (directoryPath.size() >= sizeof(dirBuf)) directoryPath.resize(sizeof(dirBuf)-1);
        std::snprintf(dirBuf, sizeof(dirBuf), "%s", directoryPath.c_str());
        if (ImGui::InputText("Directory", dirBuf, sizeof(dirBuf))) {
            directoryPath = dirBuf;
        }
        ImGui::SameLine();
        ImGui::Checkbox("Recursive", &recursive);

        if (ImGui::Button("Load")) {
            walkk.files.clear();
            loaded = (directoryPath.size() > 0) && (loadDirectoryMp3s(directoryPath.c_str(), walkk, recursive) == 0);
        }

        ImGui::Separator();
        ImGui::Text("%zu files", walkk.files.size());

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

        if (!playing) {
            if (ImGui::Button("Play") && loaded && !walkk.files.empty()) {
                int err = openAndStartStream(&stream, &callbackData, kSinkChannels, kSinkRate, 256);
                if (err == paNoError) {
                    playing = true;
                    walkk.allFinished.store(false);
                    producer = std::thread([&walkk]() { granulizerLoop(&walkk); });
                }
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
    if (stream) stopAndCloseStream(stream);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}


