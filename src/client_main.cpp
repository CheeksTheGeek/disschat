#include <iostream>
#include <string>
#include <mutex>
#include <vector>
#include <fstream>

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

static std::string g_chatLog;
static std::mutex  g_chatLogMutex;

// We store name, message, and room:
static char g_nameBuf[128] = "User";
static char g_roomBuf[64]  = "General";
static char g_msgBuf[256]  = "";

// For demo, a “File path” text box:
static char g_filePathBuf[256] = "";

// The client
static Chat::Client g_client;

void AppendToChatLog(const std::string& line) {
  std::lock_guard<std::mutex> lock(g_chatLogMutex);
  g_chatLog += line;
  g_chatLog += "\n";
}
// Global variable for debouncing
static std::chrono::steady_clock::time_point g_lastKeyCheck = std::chrono::steady_clock::now();

bool IsAnyKeyDown()
{
  // Check if enough time has passed since last key check (500ms debounce)
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastKeyCheck);
  if (elapsed.count() < 1000) return false;

  // Check for key presses
  for (ImGuiKey key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; key = static_cast<ImGuiKey>(static_cast<int>(key) + 1))
  {
    if (ImGui::IsKeyDown(key)) {
      g_lastKeyCheck = now;
      return true;
    }
  }
  return false;
}


int main() {
  try {
    glfwSetErrorCallback(glfw_error_callback);
    if(!glfwInit())
      return 1;

    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "DissChat Client - Fancy", nullptr, nullptr);
    if(!window)
      return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Setup ImGui styles
    ImGui::StyleColorsDark();

    // Platform/Renderer
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Connect to localhost:5555
    g_client.connect("127.0.0.1", 5555);

    // On incoming message, print to chat
    g_client.on_incoming_message = [](const ChatMessage& msg){
      if(msg.isFile) {
        // Could prompt user to save
        AppendToChatLog("[FILE from " + msg.sender + " in " + msg.room + "]: " + msg.fileName +
                        " (size=" + std::to_string(msg.fileData.size()) + ")");
      } else if(msg.isTyping && msg.sender != g_nameBuf) {
        // Show “X is typing...”
        AppendToChatLog("[Typing]: " + msg.text);
      } else {
        // Normal chat
        AppendToChatLog("[" + msg.room + "] " + msg.sender + ": " + msg.text);
      }
    };

    bool send_button_pressed = false;
    bool send_file_button_pressed = false;
    bool done = false;
    while(!glfwWindowShouldClose(window) && !done) {
      glfwPollEvents();

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      // ~~~~~ Chat Window ~~~~~
      ImGui::Begin("DissChat - Fancy Features");

      ImGui::InputText("Name", g_nameBuf, IM_ARRAYSIZE(g_nameBuf));
      ImGui::InputText("Room", g_roomBuf, IM_ARRAYSIZE(g_roomBuf));

      // Button to “Join Room”
      ImGui::SameLine();
      if(ImGui::Button("Join")) {
        // We send a special command: "/join MyRoom"
        ChatMessage msg;
        msg.sender = g_nameBuf;
        msg.text   = std::string("/join ") + g_roomBuf;
        g_client.send(msg);
      }

      
      
      // If user typed in msg, plus "Send"
      ImGui::InputText("Message", g_msgBuf, IM_ARRAYSIZE(g_msgBuf), ImGuiInputTextFlags_EnterReturnsTrue);
      ImGui::SameLine();
      send_button_pressed = ImGui::Button("Send");
      // File sharing
      ImGui::InputText("File path", g_filePathBuf, IM_ARRAYSIZE(g_filePathBuf));
      ImGui::SameLine();
      send_file_button_pressed = ImGui::Button("Send File");
      
      if(send_button_pressed) {
        std::string m(g_msgBuf);
        if(m.empty()) {
          AppendToChatLog("[Client] Cannot send empty message!");
        } else {
          ChatMessage chat;
          chat.sender = g_nameBuf;
          chat.room   = g_roomBuf;
          chat.text   = m;
          chat.isTyping = false;
          g_client.send(chat);
          g_msgBuf[0] = '\0';
        }
      } else if(send_file_button_pressed) {
        std::string path(g_filePathBuf);
        if(!path.empty()) {
          // read the file from disk
          std::ifstream fin(path, std::ios::binary);
          if(!fin) {
            AppendToChatLog("[Error] Could not open file: " + path);
          } else {
            std::vector<uint8_t> data((std::istreambuf_iterator<char>(fin)),
                                       std::istreambuf_iterator<char>());
            ChatMessage fileMsg;
            fileMsg.sender  = g_nameBuf;
            fileMsg.room    = g_roomBuf;
            fileMsg.isFile  = true;
            fileMsg.fileName = path; // simplistic
            fileMsg.fileData = std::move(data);
            fileMsg.isTyping = false;
            g_client.send(fileMsg);
            AppendToChatLog("[Client] Sent file: " + path);
          }
        } else {
          AppendToChatLog("[Client] No file path specified!");
        }
      }
      // else if (IsAnyKeyDown()) { // Check if any key is pressed
      //     ChatMessage typing;
      //     typing.sender = g_nameBuf;
      //     typing.room = g_roomBuf;
      //     typing.isTyping = true;
      //     typing.text = g_nameBuf;
      //     g_client.send(typing);
      // }
    
      ImGui::Separator();

      // A big chat log area
      ImGui::TextUnformatted("Chat Log:");
      ImGui::BeginChild("ChatLog", ImVec2(0, 0), true);
      {
        std::lock_guard<std::mutex> lock(g_chatLogMutex);
        ImGui::TextUnformatted(g_chatLog.c_str());
        if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f) {
          ImGui::SetScrollHereY(1.0f);
        }
      }
      ImGui::EndChild();

      ImGui::End();

      // Render
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

    // Basic checks
    if (DEBUGGING) std::cout << "Testing Boost threading..." << std::endl;
    boost::thread testThread([](){
      if (DEBUGGING) std::cout << "Boost thread is working!\n";
    });
    testThread.join();

    if (DEBUGGING)  std::cout << "Testing Taskflow concurrency...\n";
    tf::Taskflow tfFlow;
    tfFlow.emplace([](){if (DEBUGGING)  std::cout << "Taskflow is working!\n"; });
    tf::Executor ex;
    ex.run(tfFlow).wait();

    return 0;
  }
  catch(const std::exception& e) {
    std::cerr << "[Client] Error: " << e.what() << std::endl;
    return 1;
  }
}
