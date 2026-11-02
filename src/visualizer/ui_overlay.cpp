#include "visualizer/ui_overlay.h"
#include "visualizer/imgui/imgui_impl_sdl3.h"
#include "visualizer/imgui/imgui_impl_vulkan.h"
#include "visualizer/trace_visual_adapter.h"
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>

static void check_vk_result(VkResult err) {
  if (err == 0) return;
  std::cerr << "[ImGui Vulkan] Error: VkResult = " << err << "\n";
  if (err < 0) abort();
}

UIOverlay::UIOverlay(SDL_Window *window, vk::Instance instance,
                     vk::PhysicalDevice physicalDevice, vk::Device device,
                     uint32_t queueFamily, vk::Queue queue,
                     vk::RenderPass renderPass, uint32_t imageCount,
                     const std::string &sourceFilePath)
    : m_window(window), m_device(device), m_sourcePath(sourceFilePath) {
  loadSourceFile(sourceFilePath);

  // Dedicated Descriptor Pool for ImGui
  std::array<vk::DescriptorPoolSize, 11> pool_sizes = {
      {{vk::DescriptorType::eSampler, 1000},
       {vk::DescriptorType::eCombinedImageSampler, 1000},
       {vk::DescriptorType::eSampledImage, 1000},
       {vk::DescriptorType::eStorageImage, 1000},
       {vk::DescriptorType::eUniformTexelBuffer, 1000},
       {vk::DescriptorType::eStorageTexelBuffer, 1000},
       {vk::DescriptorType::eUniformBuffer, 1000},
       {vk::DescriptorType::eStorageBuffer, 1000},
       {vk::DescriptorType::eUniformBufferDynamic, 1000},
       {vk::DescriptorType::eStorageBufferDynamic, 1000},
       {vk::DescriptorType::eInputAttachment, 1000}}};

  vk::DescriptorPoolCreateInfo pool_info(
      vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1000 * 11,
      static_cast<uint32_t>(pool_sizes.size()), pool_sizes.data());
  m_imguiDescriptorPool = device.createDescriptorPool(pool_info);

  // Initialize ImGui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  applyModernWebTheme();

  // Load crisp fonts with High-DPI scaling
  {
    float xscale = 1.0f, yscale = 1.0f;
    if (window) {
      float scale = SDL_GetWindowDisplayScale(window);
      if (scale > 0.0f) xscale = yscale = scale;
    }
    float dpi = std::max(xscale, yscale);
    if (dpi < 1.0f) dpi = 1.0f;

    ImFontConfig fontCfg;
    fontCfg.OversampleH = 3;
    fontCfg.OversampleV = 2;
    fontCfg.PixelSnapH = true;

    // 1. Primary UI Font
    const char *uiFontPaths[] = {
        "/usr/share/fonts/Adwaita/AdwaitaSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    };
    for (const char *path : uiFontPaths) {
      std::ifstream probe(path);
      if (probe.good()) {
        io.Fonts->AddFontFromFileTTF(path, std::round(15.0f * dpi), &fontCfg);
        break;
      }
    }

    // 2. Monospace Font for Code & Stacks
    const char *monoFontPaths[] = {
        "/usr/share/fonts/TTF/JetBrainsMonoNerdFont-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/Adwaita/AdwaitaMono-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    };
    for (const char *path : monoFontPaths) {
      std::ifstream probe(path);
      if (probe.good()) {
        m_monoFont = io.Fonts->AddFontFromFileTTF(path, std::round(14.0f * dpi), &fontCfg);
        break;
      }
    }

    if (!m_monoFont) {
      m_monoFont = io.Fonts->AddFontDefault();
    }

    if (dpi > 1.0f) {
      ImGui::GetStyle().ScaleAllSizes(dpi);
    }
  }

  // Setup Platform/Renderer backends
  ImGui_ImplSDL3_InitForVulkan(window);

  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.ApiVersion = VK_API_VERSION_1_2;
  init_info.Instance = instance;
  init_info.PhysicalDevice = physicalDevice;
  init_info.Device = device;
  init_info.QueueFamily = queueFamily;
  init_info.Queue = queue;
  init_info.PipelineCache = VK_NULL_HANDLE;
  init_info.DescriptorPool = m_imguiDescriptorPool;
  init_info.PipelineInfoMain.RenderPass = renderPass;
  init_info.PipelineInfoMain.Subpass = 0;
  init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.MinImageCount = imageCount;
  init_info.ImageCount = imageCount;
  init_info.CheckVkResultFn = check_vk_result;

  ImGui_ImplVulkan_Init(&init_info);
}

UIOverlay::~UIOverlay() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  if (m_imguiDescriptorPool) {
    m_device.destroyDescriptorPool(m_imguiDescriptorPool);
  }
}

void UIOverlay::applyModernWebTheme() {
  ImGuiStyle &style = ImGui::GetStyle();
  ImVec4 *colors = style.Colors;

  // Solid, clean geometry
  style.WindowRounding = 8.0f;
  style.ChildRounding = 6.0f;
  style.FrameRounding = 6.0f;
  style.PopupRounding = 6.0f;
  style.ScrollbarRounding = 6.0f;
  style.GrabRounding = 4.0f;
  style.TabRounding = 6.0f;

  style.WindowBorderSize = 1.0f;
  style.ChildBorderSize = 1.0f;
  style.FrameBorderSize = 1.0f;

  style.WindowPadding = ImVec2(12.0f, 12.0f);
  style.FramePadding = ImVec2(10.0f, 6.0f);
  style.ItemSpacing = ImVec2(8.0f, 8.0f);
  style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);

  // 100% Solid Colors Palette (Alpha = 1.0f, no hazy transparencies)
  colors[ImGuiCol_Text]                 = ImVec4(0.97f, 0.98f, 0.99f, 1.00f); // Slate 50
  colors[ImGuiCol_TextDisabled]         = ImVec4(0.58f, 0.64f, 0.72f, 1.00f); // Slate 400
  colors[ImGuiCol_WindowBg]             = ImVec4(0.06f, 0.08f, 0.13f, 1.00f); // Slate 950
  colors[ImGuiCol_ChildBg]              = ImVec4(0.09f, 0.12f, 0.19f, 1.00f); // Slate 900
  colors[ImGuiCol_PopupBg]              = ImVec4(0.09f, 0.12f, 0.19f, 1.00f);
  colors[ImGuiCol_Border]               = ImVec4(0.20f, 0.25f, 0.35f, 1.00f); // Slate 700
  colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_FrameBg]              = ImVec4(0.12f, 0.16f, 0.25f, 1.00f); // Slate 800
  colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.18f, 0.24f, 0.36f, 1.00f);
  colors[ImGuiCol_FrameBgActive]        = ImVec4(0.24f, 0.30f, 0.44f, 1.00f);
  colors[ImGuiCol_TitleBg]              = ImVec4(0.07f, 0.09f, 0.15f, 1.00f);
  colors[ImGuiCol_TitleBgActive]        = ImVec4(0.09f, 0.12f, 0.19f, 1.00f);
  colors[ImGuiCol_MenuBarBg]            = ImVec4(0.08f, 0.10f, 0.16f, 1.00f);
  colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.06f, 0.08f, 0.13f, 1.00f);
  colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.25f, 0.32f, 0.45f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.43f, 0.60f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.45f, 0.55f, 0.75f, 1.00f);

  // Solid Accents (Blue, Indigo, Emerald)
  colors[ImGuiCol_CheckMark]            = ImVec4(0.22f, 0.74f, 0.98f, 1.00f); // Sky 400
  colors[ImGuiCol_SliderGrab]           = ImVec4(0.25f, 0.45f, 0.95f, 1.00f);
  colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.35f, 0.55f, 1.00f, 1.00f);
  colors[ImGuiCol_Button]               = ImVec4(0.15f, 0.39f, 0.92f, 1.00f); // Solid Blue 600
  colors[ImGuiCol_ButtonHovered]        = ImVec4(0.22f, 0.48f, 0.96f, 1.00f);
  colors[ImGuiCol_ButtonActive]         = ImVec4(0.11f, 0.31f, 0.78f, 1.00f);
  colors[ImGuiCol_Header]               = ImVec4(0.18f, 0.24f, 0.36f, 1.00f);
  colors[ImGuiCol_HeaderHovered]        = ImVec4(0.25f, 0.33f, 0.48f, 1.00f);
  colors[ImGuiCol_HeaderActive]         = ImVec4(0.15f, 0.39f, 0.92f, 1.00f);
  colors[ImGuiCol_Separator]            = ImVec4(0.20f, 0.25f, 0.35f, 1.00f);
  colors[ImGuiCol_Tab]                  = ImVec4(0.12f, 0.16f, 0.25f, 1.00f);
  colors[ImGuiCol_TabHovered]           = ImVec4(0.25f, 0.33f, 0.48f, 1.00f);
  colors[ImGuiCol_TabActive]            = ImVec4(0.15f, 0.39f, 0.92f, 1.00f);
}

void UIOverlay::loadSourceFile(const std::string &path) {
  m_sourceLines.clear();
  std::ifstream file(path);
  if (!file.is_open()) {
    m_sourceLines.push_back("// (Source file could not be loaded: " + path + ")");
    return;
  }
  std::string line;
  while (std::getline(file, line)) {
    m_sourceLines.push_back(line);
  }
}

void UIOverlay::newFrame() {
  if (!m_window) return;
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
}

std::string UIOverlay::generateNarrationText(const StepRecord &step) {
  if (!step.delta.array_mutations.empty()) {
    std::string text;
    for (size_t i = 0; i < step.delta.array_mutations.size(); ++i) {
      const auto &mut = step.delta.array_mutations[i];
      if (i > 0) text += "  |  ";
      text += mut.variable_name + "[" + std::to_string(mut.index) + "]: " +
              mut.prev_value + " → " + mut.curr_value;
    }
    return "Array mutation: " + text;
  }

  if (!step.delta.modified_vars.empty()) {
    std::string changed;
    size_t shown = std::min(step.delta.modified_vars.size(), (size_t)3);
    for (size_t i = 0; i < shown; ++i) {
      if (i > 0) changed += ", ";
      const auto &varName = step.delta.modified_vars[i];
      changed += varName;
      for (const auto &v : step.variables) {
        if (v.first == varName) {
          changed += " = " + v.second.to_string();
          break;
        }
      }
    }
    if (step.delta.modified_vars.size() > 3) {
      changed += " (+" + std::to_string(step.delta.modified_vars.size() - 3) + " more)";
    }
    return "Variables updated: " + changed;
  }

  if (step.call_stack.size() > 1) {
    return "Inside " + step.function_name + "() — call depth " +
           std::to_string(step.call_stack.size());
  }

  return "Executing line " + std::to_string(step.line) + " in " + step.function_name + "()";
}

void UIOverlay::renderTopNavbar(const StepRecord &step, const UIPlaybackState &state) {
  ImGui::BeginChild("TopNavbar", ImVec2(0, 44), true, ImGuiWindowFlags_NoScrollbar);

  // Brand Badge
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.39f, 0.92f, 1.0f));
  ImGui::Button("CRIX");
  ImGui::PopStyleColor();
  ImGui::SameLine();

  // Breadcrumb
  ImGui::TextColored(ImVec4(0.58f, 0.64f, 0.72f, 1.0f), "file: %s", m_sourcePath.c_str());
  ImGui::SameLine();

  // Function badge
  ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.98f, 1.0f), "• %s() [L%d]",
                     step.function_name.c_str(), step.line);

  // Auto-discovered DSA Topology badge
  auto topo = TopologyDetector::discover(step, ExecutionTrace{}, state.selected_data_structure);
  if (!topo.description.empty()) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.96f, 0.62f, 0.04f, 1.0f), "• %s", topo.description.c_str());
  }

  // Drop Sidebar Toggle Button
  ImGui::SameLine(ImGui::GetWindowWidth() - 440.0f);
  std::string targetLabel = state.selected_data_structure.empty()
                                ? "▾ Data Structures"
                                : ("▾ Target: " + state.selected_data_structure);
  if (m_sidebarOpen) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.73f, 0.51f, 1.0f)); // Solid Emerald when open
  } else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.24f, 0.36f, 1.0f)); // Solid Slate
  }
  if (ImGui::Button(targetLabel.c_str())) {
    m_sidebarOpen = !m_sidebarOpen;
  }
  ImGui::PopStyleColor();

  // Step Status Badge
  ImGui::SameLine(ImGui::GetWindowWidth() - 200.0f);
  if (state.is_paused) {
    ImGui::TextColored(ImVec4(0.96f, 0.62f, 0.04f, 1.0f), "⏸ PAUSED");
  } else {
    ImGui::TextColored(ImVec4(0.06f, 0.73f, 0.51f, 1.0f), "● PLAYING");
  }
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.92f, 1.0f), "Step %zu/%zu",
                     state.total_steps > 0 ? (state.current_step + 1) : 0, state.total_steps);

  ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
// C Syntax Tokenizer for Code View
// ─────────────────────────────────────────────────────────────────────────────
struct CCodeToken {
  std::string text;
  ImVec4 color;
};

static std::vector<CCodeToken> tokenizeCLine(const std::string &line) {
  std::vector<CCodeToken> tokens;
  size_t i = 0;
  size_t n = line.size();

  const ImVec4 colKeyword(0.75f, 0.52f, 0.99f, 1.0f); // Solid Purple
  const ImVec4 colType(0.38f, 0.75f, 0.98f, 1.0f);    // Solid Sky
  const ImVec4 colString(0.34f, 0.80f, 0.54f, 1.0f);  // Solid Emerald
  const ImVec4 colComment(0.45f, 0.52f, 0.63f, 1.0f); // Solid Slate
  const ImVec4 colNumber(0.98f, 0.75f, 0.38f, 1.0f);  // Solid Amber
  const ImVec4 colDefault(0.95f, 0.96f, 0.98f, 1.0f); // Solid White

  static const std::unordered_set<std::string> keywords = {
      "if", "else", "for", "while", "do", "return", "switch", "case", "break",
      "continue", "typedef", "struct", "enum", "sizeof", "const", "static", "void"};
  static const std::unordered_set<std::string> types = {
      "int", "char", "float", "double", "size_t", "uint32_t", "int64_t", "bool",
      "Point", "Color", "auto"};

  while (i < n) {
    if (i + 1 < n && line[i] == '/' && line[i + 1] == '/') {
      tokens.push_back({line.substr(i), colComment});
      break;
    }
    if (line[i] == '"' || line[i] == '\'') {
      char q = line[i];
      size_t start = i++;
      while (i < n && line[i] != q) {
        if (line[i] == '\\' && i + 1 < n) i++;
        i++;
      }
      if (i < n) i++;
      tokens.push_back({line.substr(start, i - start), colString});
      continue;
    }
    if (std::isdigit(static_cast<unsigned char>(line[i]))) {
      size_t start = i++;
      while (i < n && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '.')) i++;
      tokens.push_back({line.substr(start, i - start), colNumber});
      continue;
    }
    if (std::isalpha(static_cast<unsigned char>(line[i])) || line[i] == '_') {
      size_t start = i++;
      while (i < n && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_')) i++;
      std::string word = line.substr(start, i - start);
      if (keywords.count(word)) tokens.push_back({word, colKeyword});
      else if (types.count(word)) tokens.push_back({word, colType});
      else tokens.push_back({word, colDefault});
      continue;
    }
    size_t start = i++;
    while (i < n && !std::isalnum(static_cast<unsigned char>(line[i])) &&
           line[i] != '_' && line[i] != '"' && line[i] != '\'' &&
           !(i + 1 < n && line[i] == '/' && line[i + 1] == '/')) {
      i++;
    }
    tokens.push_back({line.substr(start, i - start), colDefault});
  }
  return tokens;
}

// ─────────────────────────────────────────────────────────────────────────────
// PANE 1: Code Executing
// ─────────────────────────────────────────────────────────────────────────────
void UIOverlay::renderCodeExecutingPane(int currentLine, float width, float height) {
  ImGui::BeginChild("PaneCodeExecuting", ImVec2(width, height), true);

  ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.98f, 1.0f), "💻 CODE EXECUTING");
  ImGui::SameLine();
  ImGui::TextDisabled("| Line %d", currentLine);
  ImGui::Separator();

  ImGui::BeginChild("CodeScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

  if (m_monoFont) ImGui::PushFont(m_monoFont);

  for (size_t i = 0; i < m_sourceLines.size(); ++i) {
    int lineNum = static_cast<int>(i + 1);
    bool isActive = (lineNum == currentLine);

    if (isActive) {
      ImVec2 p_min = ImGui::GetCursorScreenPos();
      ImVec2 p_max = ImVec2(p_min.x + ImGui::GetContentRegionAvail().x,
                            p_min.y + ImGui::GetTextLineHeightWithSpacing());

      // Solid dark active row + solid blue left marker
      ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, IM_COL32(30, 41, 59, 255), 4.0f);
      ImGui::GetWindowDrawList()->AddRectFilled(p_min, ImVec2(p_min.x + 4, p_max.y),
                                                IM_COL32(59, 130, 246, 255), 2.0f);

      // Auto-scroll on step change
      if (m_lastActiveLine != currentLine) {
        ImGui::SetScrollHereY(0.4f);
        m_lastActiveLine = currentLine;
      }
    }

    if (isActive) {
      ImGui::TextColored(ImVec4(0.96f, 0.62f, 0.04f, 1.0f), " %2d ► ", lineNum);
    } else {
      ImGui::TextDisabled(" %2d │ ", lineNum);
    }

    auto tokens = tokenizeCLine(m_sourceLines[i]);
    for (size_t tIdx = 0; tIdx < tokens.size(); ++tIdx) {
      ImGui::SameLine(0, 0);
      ImGui::TextColored(tokens[tIdx].color, "%s", tokens[tIdx].text.c_str());
    }
  }

  if (m_monoFont) ImGui::PopFont();

  ImGui::EndChild();
  ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
// PANE 2: Stack Trace
// ─────────────────────────────────────────────────────────────────────────────
void UIOverlay::renderStackTracePane(const StepRecord &step, float width, float height) {
  ImGui::BeginChild("PaneStackTrace", ImVec2(width, height), true);

  ImGui::TextColored(ImVec4(0.06f, 0.73f, 0.51f, 1.0f), "🌳 STACK TRACE");
  ImGui::SameLine();
  ImGui::TextDisabled("(Depth: %zu)", step.call_stack.size());
  ImGui::Separator();

  if (step.call_stack.empty()) {
    ImGui::TextDisabled("No active stack frames.");
    ImGui::EndChild();
    return;
  }

  ImGui::BeginChild("StackFramesScroll", ImVec2(0, 0), false);

  for (size_t i = 0; i < step.call_stack.size(); ++i) {
    const auto &frame = step.call_stack[i];
    bool isTopFrame = (i == 0);

    ImGui::PushID(static_cast<int>(i));

    float cardEstimatedH = 68.0f + static_cast<float>(frame.variables.size()) * 22.0f;
    ImGui::BeginChild("FrameCard", ImVec2(0, cardEstimatedH), true);

    // Frame Badge
    if (isTopFrame) {
      ImGui::TextColored(ImVec4(0.06f, 0.73f, 0.51f, 1.0f), "▶ [Frame %zu] %s()", i, frame.name.c_str());
    } else {
      ImGui::TextColored(ImVec4(0.58f, 0.64f, 0.72f, 1.0f), "  [Frame %zu] %s()", i, frame.name.c_str());
    }

    // Source location
    std::string loc = "    at " + (frame.file.empty() ? step.file : frame.file) + ":" + std::to_string(frame.line);
    ImGui::TextDisabled("%s", loc.c_str());

    // Frame variables
    if (!frame.variables.empty()) {
      ImGui::TextColored(ImVec4(0.38f, 0.75f, 0.98f, 1.0f), "    Variables:");
      for (const auto &v : frame.variables) {
        ImGui::Text("      • %s: %s", v.first.c_str(), v.second.to_string().c_str());
      }
    }

    ImGui::EndChild();
    ImGui::PopID();
    ImGui::Spacing();
  }

  ImGui::EndChild();
  ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
// PANE 3: Animation Canvas (Modular & Data-Structure Agnostic)
// ─────────────────────────────────────────────────────────────────────────────
void UIOverlay::renderAnimationCanvasPane(const ExecutionTrace &trace,
                                         const UIPlaybackState &state,
                                         float width, float height) {
  (void)trace;
  ImGui::BeginChild("PaneAnimationCanvas", ImVec2(width, height), true);

  ImGui::TextColored(ImVec4(0.39f, 0.40f, 0.95f, 1.0f), "📊 DATA STRUCTURE ANIMATION");
  ImGui::SameLine();
  if (!state.selected_data_structure.empty()) {
    ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.98f, 1.0f), "| Target: %s",
                       state.selected_data_structure.c_str());
  } else {
    ImGui::TextDisabled("| Target: Auto");
  }
  ImGui::Separator();

  if (trace.steps.empty() || state.current_step >= trace.steps.size()) {
    ImGui::TextDisabled("No active trace step available.");
    ImGui::EndChild();
    return;
  }

  const StepRecord &step = trace.steps[state.current_step];

  // 1. Narration Card at top of Canvas
  std::string narration = generateNarrationText(step);
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.16f, 0.25f, 1.0f));
  ImGui::BeginChild("CanvasNarration", ImVec2(0, 36), true, ImGuiWindowFlags_NoScrollbar);
  ImGui::TextColored(ImVec4(0.96f, 0.62f, 0.04f, 1.0f), "⚡");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.97f, 0.98f, 0.99f, 1.0f), "%s", narration.c_str());
  ImGui::EndChild();
  ImGui::PopStyleColor();

  // 2. Stdout Card height reservation
  float stdoutH = 80.0f;
  float canvasDrawH = ImGui::GetContentRegionAvail().y - stdoutH - 8.0f;
  if (canvasDrawH < 140.0f) canvasDrawH = 140.0f;

  // 3. Render Generic VisualScene via Modular AnimationEngine
  VisualScene scene = TraceVisualAdapter::buildVisualScene(
      step, state.selected_data_structure, state.animation_progress);

  ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
  ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, canvasDrawH);
  ImDrawList *drawList = ImGui::GetWindowDrawList();

  m_animEngine.renderScene(drawList, scene, canvasOrigin, canvasSize, state.animation_progress);
  ImGui::Dummy(canvasSize); // Advance ImGui cursor past canvas

  // 4. Output Console / Stdout Card
  ImGui::BeginChild("ConsoleCard", ImVec2(0, 0), true);
  ImGui::TextColored(ImVec4(0.58f, 0.64f, 0.72f, 1.0f), "TERMINAL STDOUT");
  ImGui::Separator();
  if (m_cumulativeStdout.empty()) {
    ImGui::TextDisabled("(No stdout output recorded up to this step)");
  } else {
    if (m_monoFont) ImGui::PushFont(m_monoFont);
    ImGui::TextUnformatted(m_cumulativeStdout.c_str());
    if (m_monoFont) ImGui::PopFont();
  }
  ImGui::EndChild();

  ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
// Drop Sidebar: Select Data Structure to Observe
// ─────────────────────────────────────────────────────────────────────────────
void UIOverlay::renderDataStructureDropSidebar(const StepRecord &step,
                                              const ExecutionTrace &trace,
                                              UIPlaybackState &state) {
  if (!m_sidebarOpen) return;

  ImGuiViewport *viewport = ImGui::GetMainViewport();
  float sidebarWidth = 320.0f;
  float sidebarHeight = viewport->Size.y - 120.0f;
  ImVec2 sidebarPos(viewport->Pos.x + viewport->Size.x - sidebarWidth - 16.0f,
                    viewport->Pos.y + 56.0f);

  ImGui::SetNextWindowPos(sidebarPos, ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(sidebarWidth, sidebarHeight), ImGuiCond_Always);

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoCollapse;

  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.09f, 0.12f, 0.19f, 1.0f)); // Solid Slate 900
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.35f, 0.50f, 1.0f));   // Solid Border

  if (ImGui::Begin("Select Data Structure", &m_sidebarOpen, flags)) {
    ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.98f, 1.0f), "OBSERVED DATA STRUCTURE");
    ImGui::Separator();

    // Auto-Select Button
    bool isAuto = state.selected_data_structure.empty();
    if (isAuto) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.73f, 0.51f, 1.0f));
    } else {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.24f, 0.36f, 1.0f));
    }
    if (ImGui::Button("Auto / Active Scope", ImVec2(-1, 30))) {
      state.selected_data_structure.clear();
    }
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Call Stack / Recursion Tree Button
    bool isCallStack = (state.selected_data_structure == "call_stack");
    if (isCallStack) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.73f, 0.51f, 1.0f));
    } else {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.24f, 0.36f, 1.0f));
    }
    if (ImGui::Button("🌳 Call Stack / Recursion Tree", ImVec2(-1, 30))) {
      state.selected_data_structure = "call_stack";
    }
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Discover candidates
    auto candidates = TraceVisualAdapter::discoverDataStructures(step, trace);

    ImGui::TextDisabled("Discovered Variables & Collections (%zu):", candidates.size());
    ImGui::BeginChild("CandidatesList", ImVec2(0, 0), false);

    for (size_t i = 0; i < candidates.size(); ++i) {
      const auto &c = candidates[i];
      bool isSelected = (state.selected_data_structure == c.name);

      ImGui::PushID(static_cast<int>(i));

      if (isSelected) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.28f, 0.45f, 1.0f)); // Solid Selected Card
      } else {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.16f, 0.25f, 1.0f)); // Solid Card
      }

      ImGui::BeginChild("CandidateCard", ImVec2(0, 56), true);

      // Label & Kind Tag
      const char *kindTag = "[VAR]";
      if (c.kind == VisualNodeKind::ARRAY_CELL) kindTag = "[ARRAY]";
      else if (c.kind == VisualNodeKind::STRUCT_RECORD) kindTag = "[STRUCT]";
      else if (c.kind == VisualNodeKind::POINTER_LINK) kindTag = "[PTR]";

      ImGui::TextColored(ImVec4(0.97f, 0.98f, 0.99f, 1.0f), "%s %s", kindTag, c.name.c_str());
      ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70.0f);

      if (isSelected) {
        ImGui::TextColored(ImVec4(0.06f, 0.73f, 0.51f, 1.0f), "● ACTIVE");
      } else if (c.is_active_in_step) {
        ImGui::TextDisabled("in scope");
      } else {
        ImGui::TextDisabled("inactive");
      }

      // Type & Scope
      std::string detail = c.type;
      if (!c.scope_name.empty()) detail += "  |  " + c.scope_name;
      ImGui::TextDisabled("%s", detail.c_str());

      // Click to select
      if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
        state.selected_data_structure = c.name;
      }

      ImGui::EndChild();
      ImGui::PopStyleColor();
      ImGui::PopID();
      ImGui::Spacing();
    }

    ImGui::EndChild();
  }
  ImGui::End();
  ImGui::PopStyleColor(2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Bottom Playback Media Dock
// ─────────────────────────────────────────────────────────────────────────────
void UIOverlay::renderBottomMediaDock(UIPlaybackState &state) {
  ImGui::BeginChild("BottomMediaDock", ImVec2(0, 68), true, ImGuiWindowFlags_NoScrollbar);

  // Restart Button
  if (ImGui::Button("⏮", ImVec2(36, 32))) {
    state.restart = true;
  }
  ImGui::SameLine();

  // Step Backward Button
  if (ImGui::Button("◀", ImVec2(36, 32))) {
    state.step_backward = true;
  }
  ImGui::SameLine();

  // Play / Pause Button
  const char *playPauseLabel = state.is_paused ? "▶ Play" : "⏸ Pause";
  if (state.is_paused) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.73f, 0.51f, 1.0f)); // Solid Emerald
  } else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.96f, 0.62f, 0.04f, 1.0f)); // Solid Amber
  }
  if (ImGui::Button(playPauseLabel, ImVec2(80, 32))) {
    state.is_paused = !state.is_paused;
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();

  // Step Forward Button
  if (ImGui::Button("▶", ImVec2(36, 32))) {
    state.step_forward = true;
  }
  ImGui::SameLine();

  // Speed Selector Buttons
  ImGui::Text("  Speed:");
  ImGui::SameLine();
  const float speeds[] = {1.5f, 0.8f, 0.35f, 0.1f};
  const char *speedLabels[] = {"0.5x", "1.0x", "2.0x", "5.0x"};
  for (int sIdx = 0; sIdx < 4; ++sIdx) {
    bool isCurrent = std::abs(state.step_duration - speeds[sIdx]) < 0.01f;
    if (isCurrent) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.39f, 0.92f, 1.0f));
    } else {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.16f, 0.25f, 1.0f));
    }
    if (ImGui::Button(speedLabels[sIdx], ImVec2(46, 26))) {
      state.step_duration = speeds[sIdx];
    }
    ImGui::PopStyleColor();
    ImGui::SameLine();
  }

  // Timeline Scrubber Slider
  ImGui::SameLine();
  float sliderWidth = ImGui::GetContentRegionAvail().x - 14.0f;
  if (sliderWidth > 80.0f) {
    ImGui::PushItemWidth(sliderWidth);
    int currentStepInt = static_cast<int>(state.current_step);
    int maxStepsInt = state.total_steps > 0 ? static_cast<int>(state.total_steps - 1) : 0;
    if (ImGui::SliderInt("##Scrubber", &currentStepInt, 0, maxStepsInt, "Step %d")) {
      state.scrub_target = static_cast<size_t>(currentStepInt);
      state.scrubbed = true;
    }
    ImGui::PopItemWidth();
  }

  ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
// Root UI Dispatcher (3-Pane Architecture)
// ─────────────────────────────────────────────────────────────────────────────
void UIOverlay::renderUI(const ExecutionTrace &trace, UIPlaybackState &state) {
  state.total_steps = trace.steps.size();

  // Accumulate console output up to active step
  m_cumulativeStdout.clear();
  for (size_t i = 0; i <= state.current_step && i < trace.steps.size(); ++i) {
    if (!trace.steps[i].stdout_chunk.empty()) {
      m_cumulativeStdout += trace.steps[i].stdout_chunk;
    }
  }

  // Fullscreen container
  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);

  ImGuiWindowFlags rootFlags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::Begin("CRIXVisualizerRoot", nullptr, rootFlags);

  const StepRecord emptyStep{};
  const StepRecord &currentStep =
      trace.steps.empty() ? emptyStep : trace.steps[state.current_step];

  // 1. Top Navbar
  renderTopNavbar(currentStep, state);

  // 2. Three Panes Layout
  float totalHeight = ImGui::GetContentRegionAvail().y - 74.0f; // Leave space for media dock
  float totalWidth  = ImGui::GetContentRegionAvail().x;

  float codeWidth   = std::max(280.0f, totalWidth * 0.32f); // Pane 1: 32%
  float stackWidth  = std::max(220.0f, totalWidth * 0.24f); // Pane 2: 24%
  float canvasWidth = totalWidth - codeWidth - stackWidth - 16.0f; // Pane 3: 44%

  // PANE 1: Code Executing
  renderCodeExecutingPane(currentStep.line, codeWidth, totalHeight);
  ImGui::SameLine();

  // PANE 2: Stack Trace
  renderStackTracePane(currentStep, stackWidth, totalHeight);
  ImGui::SameLine();

  // PANE 3: Animation Canvas (Modular, Data-Structure Agnostic)
  renderAnimationCanvasPane(trace, state, canvasWidth, totalHeight);

  // Drop Sidebar (if open)
  renderDataStructureDropSidebar(currentStep, trace, state);

  // Bottom Media Dock
  renderBottomMediaDock(state);

  ImGui::End();
  ImGui::Render();
}

void UIOverlay::drawFrame(vk::CommandBuffer cmd) {
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void UIOverlay::onResize(uint32_t width, uint32_t height) {
  (void)width;
  (void)height;
}
