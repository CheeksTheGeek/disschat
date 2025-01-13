#include <iostream>
#include <string>
#include <mutex>

#include <imgui.h>
#include <boost/system/system_error.hpp>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <taskflow/taskflow.hpp>
#include <boost/thread/thread.hpp>

#include "client.hpp"

#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}



// We'll store the entire chat log in one string with newlines:
static std::string g_chatLog;
static std::mutex  g_chatLogMutex;

// Basic text buffers for Name and Message:
static char g_nameBuf[128] = "User";
static char g_msgBuf[256]  = "";

// We want to connect by default to localhost:5555
static ChatClient g_client;

void AppendToChatLog(const std::string& line) {
    std::lock_guard<std::mutex> lock(g_chatLogMutex);
    g_chatLog += line;
    g_chatLog += "\n";
}


int main() {
    try {
        // Setup window
        glfwSetErrorCallback(glfw_error_callback);
        if (!glfwInit())
            return 1;

        // GL 3.2 + GLSL 150
        const char* glsl_version = "#version 150";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // Core Profile
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required for macOS

        // Create window with graphics context
        GLFWwindow* window = glfwCreateWindow(1280, 720, "DissChat Client", nullptr, nullptr);
        if (window == nullptr)
            return 1;
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1); // Enable vsync

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init(glsl_version);

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // Connect the client to 127.0.0.1:5555
        g_client.connect("127.0.0.1", 5555);

        // Main loop
        bool done = false;
        while (!glfwWindowShouldClose(window) && !done)
        {
            glfwPollEvents();

            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            {
                // Show a simple window
                ImGui::Begin("Chat Client");

                ImGui::InputText("Name", g_nameBuf, IM_ARRAYSIZE(g_nameBuf));

                ImGui::InputText("Message", g_msgBuf, IM_ARRAYSIZE(g_msgBuf), ImGuiInputTextFlags_EnterReturnsTrue);

                ImGui::SameLine();
                if (ImGui::Button("Send") || ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter))) {
                    if (std::string(g_msgBuf).empty()) AppendToChatLog("Cannot send an empty message!");
                    else {
                        ChatMessage msg;
                        msg.sender = g_nameBuf;
                        msg.text = g_msgBuf;
                        g_client.send(msg);
                        g_msgBuf[0] = '\0';
                    }
                }

                ImGui::Separator();

                ImGui::TextUnformatted("Chat Log:");
                ImGui::BeginChild("ChatLog", ImVec2(0, 0), true);
                {
                    std::lock_guard<std::mutex> lock(g_chatLogMutex);
                    ImGui::TextUnformatted(g_chatLog.c_str());
                    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f) 
                        ImGui::SetScrollHereY(1.0f);
                }
                ImGui::EndChild();

                ImGui::End();
            }
            // Rendering
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
        }

        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();

        // Test Boost
        std::cout << "Testing Boost..." << std::endl;
        boost::thread test_thread([](){
            std::cout << "Boost thread is working!" << std::endl;
        });
        test_thread.join();

        // Test Taskflow
        std::cout << "Testing Taskflow..." << std::endl;
        tf::Taskflow taskflow;
        taskflow.emplace([](){ std::cout << "Taskflow is working!" << std::endl; });
        tf::Executor executor;
        executor.run(taskflow).wait();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }
}
