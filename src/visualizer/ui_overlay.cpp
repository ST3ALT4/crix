#include "visualizer/ui_overlay.h"
#include "visualizer/imgui/imgui_impl_glfw.h"
#include "visualizer/imgui/imgui_impl_vulkan.h"
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>

static void check_vk_result(VkResult err) {
  if (err == 0)
    return;
  std::cerr << "[ImGui Vulkan] Error: VkResult = " << err << "\n";
  if (err < 0)
    abort();
}

UIOverlay::UIOverlay(GLFWwindow *window, vk::Instance instance,
                     vk::PhysicalDevice physicalDevice, vk::Device device,
                     uint32_t queueFamily, vk::Queue queue,
                     vk::RenderPass renderPass, uint32_t imageCount,
                     const std::string &sourceFilePath)
    : m_window(window), m_device(device), m_sourcePath(sourceFilePath) {
  loadSourceFile(sourceFilePath);

  // Create dedicated Descriptor Pool for ImGui
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

  // Load crisp TrueType fonts with High-DPI scaling and subpixel oversampling
  {
    float xscale = 1.0f, yscale = 1.0f;
    if (window) {
      glfwGetWindowContentScale(window, &xscale, &yscale);
    }
    float dpi = std::max(xscale, yscale);
    if (dpi < 1.0f)
      dpi = 1.0f;

    ImFontConfig fontCfg;
    fontCfg.OversampleH = 3;
    fontCfg.OversampleV = 2;
    fontCfg.PixelSnapH = true;

    // 1. Primary UI Font (Modern clean sans-serif)
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

    // 2. Monospace Font for Code Viewer & Console (JetBrains Mono / DejaVu Sans
    // Mono)
    const char *monoFontPaths[] = {
        "/usr/share/fonts/TTF/JetBrainsMonoNerdFont-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/Adwaita/AdwaitaMono-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    };
    for (const char *path : monoFontPaths) {
      std::ifstream probe(path);
      if (probe.good()) {
        m_monoFont = io.Fonts->AddFontFromFileTTF(path, std::round(15.0f * dpi),
                                                  &fontCfg);
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
  ImGui_ImplGlfw_InitForVulkan(window, true);

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
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  if (m_imguiDescriptorPool) {
    m_device.destroyDescriptorPool(m_imguiDescriptorPool);
  }
}

void UIOverlay::applyModernWebTheme() {
  ImGuiStyle &style = ImGui::GetStyle();
  ImVec4 *colors = style.Colors;

  // Modern rounded web geometry
  style.WindowRounding = 12.0f;
  style.ChildRounding = 10.0f;
  style.FrameRounding = 8.0f;
  style.PopupRounding = 8.0f;
  style.ScrollbarRounding = 8.0f;
  style.GrabRounding = 6.0f;
  style.TabRounding = 8.0f;

  style.WindowBorderSize = 1.0f;
  style.ChildBorderSize = 1.0f;
  style.FrameBorderSize = 1.0f;

  style.WindowPadding = ImVec2(16.0f, 16.0f);
  style.FramePadding = ImVec2(10.0f, 6.0f);
  style.ItemSpacing = ImVec2(10.0f, 8.0f);
  style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);

  // Tailwind Slate-950 and elevated dark surface colors
  colors[ImGuiCol_Text] = ImVec4(0.97f, 0.98f, 0.99f, 1.00f); // Slate 50
  colors[ImGuiCol_TextDisabled] =
      ImVec4(0.58f, 0.64f, 0.72f, 1.00f);                         // Slate 400
  colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.08f, 0.13f, 0.98f); // Slate 950
  colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.12f, 0.19f, 0.90f);  // Gray 900
  colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.14f, 0.22f, 0.98f);
  colors[ImGuiCol_Border] = ImVec4(0.18f, 0.23f, 0.32f, 0.80f); // Slate 800
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.15f, 0.24f, 0.80f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.16f, 0.21f, 0.33f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.26f, 0.40f, 1.00f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.07f, 0.09f, 0.15f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.09f, 0.12f, 0.19f, 1.00f);
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.10f, 0.16f, 1.00f);
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.06f, 0.08f, 0.13f, 0.50f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.28f, 0.42f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.38f, 0.56f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.38f, 0.48f, 0.70f, 1.00f);

  // Modern Electric Indigo & Sky accents
  colors[ImGuiCol_CheckMark] = ImVec4(0.38f, 0.75f, 0.98f, 1.00f); // Sky 400
  colors[ImGuiCol_SliderGrab] =
      ImVec4(0.39f, 0.40f, 0.95f, 1.00f); // Indigo 500
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.50f, 0.52f, 0.98f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.31f, 0.27f, 0.90f, 0.85f); // Indigo 600
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.39f, 0.40f, 0.95f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.26f, 0.22f, 0.79f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.18f, 0.23f, 0.36f, 0.80f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.32f, 0.48f, 0.90f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.31f, 0.27f, 0.90f, 1.00f);
  colors[ImGuiCol_Separator] = ImVec4(0.18f, 0.23f, 0.32f, 0.80f);
  colors[ImGuiCol_Tab] = ImVec4(0.11f, 0.15f, 0.24f, 0.80f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.32f, 0.48f, 1.00f);
  colors[ImGuiCol_TabActive] = ImVec4(0.31f, 0.27f, 0.90f, 1.00f);
}

void UIOverlay::loadSourceFile(const std::string &path) {
  m_sourceLines.clear();
  std::ifstream file(path);
  if (!file.is_open()) {
    m_sourceLines.push_back("// (Source file could not be loaded: " + path +
                            ")");
    return;
  }
  std::string line;
  while (std::getline(file, line)) {
    m_sourceLines.push_back(line);
  }
}

void UIOverlay::newFrame() {
  if (!m_window)
    return; // Guard: window destroyed before overlay
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

std::string UIOverlay::generateNarrationText(const StepRecord &step) {
  // Priority 1: Array mutations — the most visually interesting events
  if (!step.delta.array_mutations.empty()) {
    std::string text;
    for (size_t i = 0; i < step.delta.array_mutations.size(); ++i) {
      const auto &mut = step.delta.array_mutations[i];
      if (i > 0)
        text += "  |  ";
      text += mut.variable_name + "[" + std::to_string(mut.index) +
              "]: " + mut.prev_value + " → " + mut.curr_value;
    }
    return "Array mutation: " + text;
  }

  // Priority 2: Variable value changes
  if (!step.delta.modified_vars.empty()) {
    std::string changed;
    size_t shown = std::min(step.delta.modified_vars.size(), (size_t)3);
    for (size_t i = 0; i < shown; ++i) {
      if (i > 0)
        changed += ", ";
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
      changed += " (+" + std::to_string(step.delta.modified_vars.size() - 3) +
                 " more)";
    }
    return "Variables updated: " + changed;
  }

  // Priority 3: Deep call stack — mention the function context
  if (step.call_stack.size() > 1) {
    return "Inside " + step.function_name + "() — call depth " +
           std::to_string(step.call_stack.size());
  }

  // Default: describe the current execution point
  return "Executing line " + std::to_string(step.line) + " in " +
         step.function_name + "()";
}

void UIOverlay::renderTopNavbar(const StepRecord &step,
                                const UIPlaybackState &state) {
  ImGui::BeginChild("TopNavbar", ImVec2(0, 48), true,
                    ImGuiWindowFlags_NoScrollbar);

  // Logo / Brand Badge
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.39f, 0.40f, 0.95f, 1.0f));
  ImGui::Button("CRIX");
  ImGui::PopStyleColor();
  ImGui::SameLine();

  // Breadcrumb
  ImGui::TextColored(ImVec4(0.58f, 0.64f, 0.72f, 1.0f), "tests / %s",
                     m_sourcePath.c_str());
  ImGui::SameLine();

  // Function badge
  ImGui::TextColored(ImVec4(0.38f, 0.75f, 0.98f, 1.0f), "• %s() [L%d]",
                     step.function_name.c_str(), step.line);

  // Status pills on right
  float rightAlign = ImGui::GetWindowWidth() - 260.0f;
  if (rightAlign > ImGui::GetCursorPosX()) {
    ImGui::SameLine(rightAlign);
  }

  if (state.is_paused) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.62f, 0.04f, 1.0f));
    ImGui::Text("⏸ PAUSED");
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.06f, 0.73f, 0.51f, 1.0f));
    ImGui::Text("● PLAYING");
  }
  ImGui::PopStyleColor();

  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.85f, 1.0f), "Step %zu / %zu",
                     state.total_steps > 0 ? (state.current_step + 1) : 0,
                     state.total_steps);

  ImGui::EndChild();
}

struct CCodeToken {
  std::string text;
  ImVec4 color;
};

static std::vector<CCodeToken> tokenizeCLine(const std::string &line) {
  std::vector<CCodeToken> tokens;
  size_t i = 0;
  size_t n = line.size();

  // VS Code / Monaco Dark+ syntax palette
  const ImVec4 colDefault(0.85f, 0.88f, 0.92f,
                          1.0f); // Slate 200 (variables, identifiers)
  const ImVec4 colKeyword(0.35f, 0.65f, 0.98f,
                          1.0f); // Electric Blue (control keywords)
  const ImVec4 colType(0.30f, 0.82f, 0.76f, 1.0f); // Teal / Mint (types)
  const ImVec4 colString(0.96f, 0.62f, 0.35f,
                         1.0f); // Warm Peach / Amber (strings & chars)
  const ImVec4 colComment(0.42f, 0.58f, 0.42f, 1.0f); // Sage Green (comments)
  const ImVec4 colNumber(0.71f, 0.85f, 0.65f,
                         1.0f); // Soft Olive Green (numbers)
  const ImVec4 colPreproc(0.78f, 0.52f, 0.85f,
                          1.0f); // Lavender / Purple (#include, #define)

  static const std::unordered_set<std::string> keywords = {
      "if",     "else",     "for",     "while",    "do",      "return",
      "break",  "continue", "switch",  "case",     "default", "goto",
      "sizeof", "const",    "static",  "volatile", "extern",  "register",
      "auto",   "inline",   "restrict"};

  static const std::unordered_set<std::string> types = {
      "int",      "float",   "double",   "char",    "void",     "short",
      "long",     "signed",  "unsigned", "bool",    "size_t",   "int8_t",
      "int16_t",  "int32_t", "int64_t",  "uint8_t", "uint16_t", "uint32_t",
      "uint64_t", "struct",  "enum",     "union",   "typedef",  "FILE",
      "NULL",     "true",    "false",    "Point",   "Color",    "Node"};

  while (i < n) {
    // Single-line comment
    if (i + 1 < n && line[i] == '/' && line[i + 1] == '/') {
      tokens.push_back({line.substr(i), colComment});
      break;
    }

    // Preprocessor directive
    if (line[i] == '#' &&
        (tokens.empty() ||
         tokens.back().text.find_first_not_of(" \t") == std::string::npos)) {
      size_t start = i;
      while (i < n && !std::isspace(static_cast<unsigned char>(line[i])))
        i++;
      tokens.push_back({line.substr(start, i - start), colPreproc});
      continue;
    }

    // String literal ("..." or '<...>')
    if (line[i] == '"' || line[i] == '\'') {
      char quote = line[i];
      size_t start = i++;
      while (i < n && line[i] != quote) {
        if (line[i] == '\\' && i + 1 < n)
          i++;
        i++;
      }
      if (i < n)
        i++;
      tokens.push_back({line.substr(start, i - start), colString});
      continue;
    }

    // Number literal (decimal, float, hex)
    if (std::isdigit(static_cast<unsigned char>(line[i]))) {
      size_t start = i;
      while (i < n && (std::isalnum(static_cast<unsigned char>(line[i])) ||
                       line[i] == '.'))
        i++;
      tokens.push_back({line.substr(start, i - start), colNumber});
      continue;
    }

    // Identifier, keyword, or type
    if (std::isalpha(static_cast<unsigned char>(line[i])) || line[i] == '_') {
      size_t start = i;
      while (i < n && (std::isalnum(static_cast<unsigned char>(line[i])) ||
                       line[i] == '_'))
        i++;
      std::string word = line.substr(start, i - start);
      if (keywords.count(word)) {
        tokens.push_back({word, colKeyword});
      } else if (types.count(word)) {
        tokens.push_back({word, colType});
      } else {
        tokens.push_back({word, colDefault});
      }
      continue;
    }

    // Whitespace or operators/punctuation
    size_t start = i;
    while (i < n && !std::isalnum(static_cast<unsigned char>(line[i])) &&
           line[i] != '_' && line[i] != '"' && line[i] != '\'' &&
           !(i + 1 < n && line[i] == '/' && line[i + 1] == '/')) {
      i++;
    }
    tokens.push_back({line.substr(start, i - start), colDefault});
  }

  return tokens;
}

void UIOverlay::renderCodeViewCard(int currentLine, float height) {
  ImGui::BeginChild("CodeCard", ImVec2(0, height), true);

  ImGui::TextColored(ImVec4(0.39f, 0.40f, 0.95f, 1.0f), "💻 SOURCE CODE");
  ImGui::SameLine();
  ImGui::TextDisabled("(Monaco Web Editor Style)");
  ImGui::Separator();

  ImGui::BeginChild("CodeContent", ImVec2(0, 0), false,
                    ImGuiWindowFlags_HorizontalScrollbar);

  if (m_monoFont)
    ImGui::PushFont(m_monoFont);

  // Time-based smooth breathing pulse for active line
  float pulse =
      0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 4.5f);

  for (size_t i = 0; i < m_sourceLines.size(); ++i) {
    int lineNum = static_cast<int>(i + 1);
    bool isActive = (lineNum == currentLine);

    if (isActive) {
      int bgAlpha = static_cast<int>(40 + 35 * pulse);
      int barAlpha = static_cast<int>(200 + 55 * pulse);

      // Accent glow background on active line with breathing pulse
      ImVec2 p_min = ImGui::GetCursorScreenPos();
      ImVec2 p_max = ImVec2(p_min.x + ImGui::GetContentRegionAvail().x,
                            p_min.y + ImGui::GetTextLineHeightWithSpacing());
      ImGui::GetWindowDrawList()->AddRectFilled(
          p_min, p_max, IM_COL32(99, 102, 241, bgAlpha), 4.0f);
      ImGui::GetWindowDrawList()->AddRectFilled(
          p_min, ImVec2(p_min.x + 4, p_max.y),
          IM_COL32(129, 140, 248, barAlpha), 2.0f);

      // Auto-scroll on line change
      if (m_lastActiveLine != currentLine) {
        ImGui::SetScrollHereY(0.5f);
        m_lastActiveLine = currentLine;
      }
    }

    // Line number gutter
    if (isActive) {
      ImVec4 activeArrowColor(0.96f + 0.04f * pulse, 0.62f + 0.2f * pulse,
                              0.04f + 0.3f * pulse, 1.0f);
      ImGui::TextColored(activeArrowColor, " %2d ➜ ", lineNum);
    } else {
      ImGui::TextDisabled(" %2d │ ", lineNum);
    }

    // Render line with C syntax highlighting
    auto tokens = tokenizeCLine(m_sourceLines[i]);
    for (size_t tIdx = 0; tIdx < tokens.size(); ++tIdx) {
      ImGui::SameLine(0, 0);
      ImVec4 col = tokens[tIdx].color;
      if (isActive && col.w == 1.0f) {
        col.x = std::min(1.0f, col.x * 1.15f);
        col.y = std::min(1.0f, col.y * 1.15f);
        col.z = std::min(1.0f, col.z * 1.15f);
      }
      ImGui::TextColored(col, "%s", tokens[tIdx].text.c_str());
    }
  }

  if (m_monoFont)
    ImGui::PopFont();

  ImGui::EndChild();
  ImGui::EndChild();
}

void UIOverlay::renderVariablesCard(const StepRecord &step, float height,
                                    float animProgress) {
  ImGui::BeginChild("VariablesCard", ImVec2(0, height), true);

  ImGui::TextColored(ImVec4(0.38f, 0.75f, 0.98f, 1.0f), "🔍 VARIABLES & SCOPE");
  ImGui::Separator();

  // Flash factor: 1.0 right after step, decaying to 0.0
  float flashIntensity = 1.0f - std::clamp(animProgress, 0.0f, 1.0f);

  if (ImGui::BeginTable("VarsTable", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 75.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableHeadersRow();

    for (const auto &v : step.variables) {
      ImGui::TableNextRow();

      // Check if modified in this step
      bool isMod = false;
      for (const auto &m : step.delta.modified_vars) {
        if (m == v.first) {
          isMod = true;
          break;
        }
      }

      // Flash background highlight on modified variable row
      if (isMod) {
        ImU32 rowFlash =
            IM_COL32(16, 185, 129, static_cast<int>(25 + 75 * flashIntensity));
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, rowFlash);
      }

      ImGui::TableSetColumnIndex(0);
      ImGui::TextColored(ImVec4(0.97f, 0.98f, 0.99f, 1.0f), "%s",
                         v.first.c_str());

      ImGui::TableSetColumnIndex(1);
      ImGui::TextDisabled("%s", v.second.type_name.c_str());

      ImGui::TableSetColumnIndex(2);
      if (isMod) {
        ImVec4 valColor(0.96f, 0.62f + 0.38f * flashIntensity,
                        0.04f + 0.5f * flashIntensity, 1.0f);
        ImGui::TextColored(valColor, "%s", v.second.to_string().c_str());
      } else {
        ImGui::Text("%s", v.second.to_string().c_str());
      }

      ImGui::TableSetColumnIndex(3);
      if (isMod) {
        ImU32 chipBg =
            IM_COL32(16, 185, 129, static_cast<int>(40 + 90 * flashIntensity));
        ImU32 chipBorder = IM_COL32(
            52, 211, 153, static_cast<int>(100 + 155 * flashIntensity));
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 chipSize(80.0f, ImGui::GetTextLineHeight() + 4.0f);
        ImGui::GetWindowDrawList()->AddRectFilled(
            p0, ImVec2(p0.x + chipSize.x, p0.y + chipSize.y), chipBg, 4.0f);
        ImGui::GetWindowDrawList()->AddRect(
            p0, ImVec2(p0.x + chipSize.x, p0.y + chipSize.y), chipBorder, 4.0f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
        ImGui::TextColored(ImVec4(0.2f + 0.8f * flashIntensity, 1.0f,
                                  0.5f + 0.5f * flashIntensity, 1.0f),
                           "● UPDATED");
      } else {
        ImGui::TextDisabled("-");
      }
    }
    ImGui::EndTable();
  }

  // Call stack breadcrumb
  if (step.call_stack.size() > 1) {
    ImGui::Spacing();
    ImGui::TextDisabled("Call Stack:");
    ImGui::SameLine();
    for (size_t i = 0; i < step.call_stack.size(); ++i) {
      if (i > 0)
        ImGui::SameLine(), ImGui::TextDisabled("➜");
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.39f, 0.40f, 0.95f, 1.0f), "%s()",
                         step.call_stack[i].name.c_str());
    }
  }

  ImGui::EndChild();
}

void UIOverlay::renderStepNarrationCard(const StepRecord &step,
                                        size_t stepIndex) {
  (void)stepIndex;
  ImGui::BeginChild("NarrationCard", ImVec2(0, 68), true);

  // Modern Web Alert Callout Banner
  std::string text = generateNarrationText(step);

  ImVec2 p_min = ImGui::GetCursorScreenPos();
  ImVec2 p_max =
      ImVec2(p_min.x + ImGui::GetContentRegionAvail().x, p_min.y + 42);

  // Background card with colored left border
  ImU32 accentColor = IM_COL32(245, 158, 11, 255); // Amber
  if (!step.delta.array_mutations.empty()) {
    accentColor = IM_COL32(244, 63, 94, 255); // Rose (Swap)
  }
  ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max,
                                            IM_COL32(30, 41, 59, 180), 8.0f);
  ImGui::GetWindowDrawList()->AddRectFilled(p_min, ImVec2(p_min.x + 6, p_max.y),
                                            accentColor, 4.0f);

  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12);
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);

  ImGui::TextColored(ImVec4(0.96f, 0.62f, 0.04f, 1.0f),
                     "💡 ACTION EXPLANATION:");
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12);
  ImGui::TextColored(ImVec4(0.95f, 0.96f, 0.98f, 1.0f), "%s", text.c_str());

  ImGui::EndChild();
}

void UIOverlay::renderArrayCanvasCard(const ExecutionTrace &trace,
                                      const UIPlaybackState &state,
                                      float height) {
  ImGui::BeginChild("CanvasCard", ImVec2(0, height), true);

  ImGui::TextColored(ImVec4(0.39f, 0.40f, 0.95f, 1.0f),
                     "📊 ALGORITHM ARRAY CARDS & POINTERS");
  ImGui::SameLine();
  ImGui::TextDisabled("(Animated Web Card View)");
  ImGui::Separator();

  if (trace.steps.empty() || state.current_step >= trace.steps.size()) {
    ImGui::TextDisabled("No active trace step available.");
    ImGui::EndChild();
    return;
  }

  const StepRecord &step = trace.steps[state.current_step];
  const StepRecord *prevStep =
      (state.current_step > 0) ? &trace.steps[state.current_step - 1] : nullptr;

  // 1. Collect all active arrays in current step for multi-array support
  struct ArrayTarget {
    std::string name;
    const RuntimeValue *val;
  };
  std::vector<ArrayTarget> activeArrays;
  for (const auto &v : step.variables) {
    if (v.second.kind == ValueKind::ARRAY && !v.second.elements.empty()) {
      activeArrays.push_back({v.first, &v.second});
    }
  }

  if (activeArrays.empty()) {
    ImGui::TextDisabled("No active array in current scope.");
    ImGui::EndChild();
    return;
  }

  // Animation progress and smooth easing
  float t = std::clamp(state.animation_progress, 0.0f, 1.0f);
  float easeT = t * t * (3.0f - 2.0f * t); // Smoothstep curve

  // Palette for iterators
  const ImU32 iterColors[] = {
      IM_COL32(245, 158, 11, 255), // Amber
      IM_COL32(16, 185, 129, 255), // Emerald
      IM_COL32(56, 189, 248, 255), // Sky
      IM_COL32(168, 85, 247, 255), // Purple
  };
  const int numIterColors = 4;

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  ImVec2 fullCanvasOrigin = ImGui::GetCursorScreenPos();
  ImVec2 fullCanvasSize = ImGui::GetContentRegionAvail();

  size_t numArrays = activeArrays.size();
  float sectionHeight = fullCanvasSize.y / static_cast<float>(numArrays);

  for (size_t arrIdx = 0; arrIdx < numArrays; ++arrIdx) {
    const auto &arrEntry = activeArrays[arrIdx];
    const std::string &arrName = arrEntry.name;
    const RuntimeValue *arrVal = arrEntry.val;
    size_t n = arrVal->elements.size();

    // Compute track bounds for this array
    float sectionTop =
        fullCanvasOrigin.y + static_cast<float>(arrIdx) * sectionHeight;
    float baseY = sectionTop + sectionHeight * 0.60f;

    // Multi-array label chip if more than one array is displayed
    if (numArrays > 1) {
      std::string headerLabel =
          "Array: " + arrName + " (" + arrVal->type_name + ")";
      drawList->AddText(ImVec2(fullCanvasOrigin.x + 8, sectionTop + 4),
                        IM_COL32(148, 163, 184, 255), headerLabel.c_str());
    }

    // Extract iterator variables valid for this array
    struct IteratorInfo {
      std::string name;
      int value;
    };
    std::vector<IteratorInfo> iterators;
    for (const auto &v : step.variables) {
      if (v.second.kind == ValueKind::PRIMITIVE &&
          (v.second.type_name == "int" || v.second.type_name == "size_t" ||
           v.second.type_name == "unsigned int") &&
          v.second.type_name.find("*") == std::string::npos) {
        if (v.first == arrName)
          continue;
        try {
          int val = std::stoi(v.second.raw_value);
          if (val >= 0 && (size_t)val < n) {
            iterators.push_back({v.first, val});
          }
        } catch (...) {
        }
      }
    }

    int ptr_j = -1;
    for (const auto &it : iterators) {
      if (it.name == "j") {
        ptr_j = it.value;
        break;
      }
    }

    // Detect swap mutations between two indices in this array
    int swapIdxA = -1, swapIdxB = -1;
    for (size_t m1 = 0; m1 < step.delta.array_mutations.size(); ++m1) {
      for (size_t m2 = m1 + 1; m2 < step.delta.array_mutations.size(); ++m2) {
        const auto &mutA = step.delta.array_mutations[m1];
        const auto &mutB = step.delta.array_mutations[m2];
        if (mutA.variable_name == arrName && mutB.variable_name == arrName) {
          if (mutA.curr_value == mutB.prev_value &&
              mutB.curr_value == mutA.prev_value) {
            swapIdxA = static_cast<int>(mutA.index);
            swapIdxB = static_cast<int>(mutB.index);
            break;
          }
        }
      }
      if (swapIdxA >= 0)
        break;
    }

    // Previous array values for smooth height interpolation
    const RuntimeValue *prevArrVal = nullptr;
    if (prevStep) {
      for (const auto &pv : prevStep->variables) {
        if (pv.first == arrName && pv.second.kind == ValueKind::ARRAY) {
          prevArrVal = &pv.second;
          break;
        }
      }
    }

    float cardWidth = std::min(
        90.0f, (fullCanvasSize.x - 40.0f) / static_cast<float>(n) - 16.0f);
    float startX = fullCanvasOrigin.x +
                   (fullCanvasSize.x - (cardWidth + 16.0f) * n) * 0.5f;

    auto getSlotX = [&](int idx) -> float {
      return startX + static_cast<float>(idx) * (cardWidth + 16.0f);
    };

    for (size_t i = 0; i < n; ++i) {
      float currVal = 1.0f;
      try {
        currVal = std::stof(arrVal->elements[i].raw_value);
      } catch (...) {
      }

      float prevVal = currVal;
      if (prevArrVal && i < prevArrVal->elements.size()) {
        try {
          prevVal = std::stof(prevArrVal->elements[i].raw_value);
        } catch (...) {
        }
      }

      // Smooth height interpolation
      float interpVal = prevVal + (currVal - prevVal) * easeT;
      float cardHeight = std::max(45.0f, interpVal * 22.0f);

      // Resting slot X position
      float slotX = getSlotX(static_cast<int>(i));
      float cardX = slotX;
      float cardY = baseY - cardHeight;

      // Swap slide animation with parabolic arc lift
      if (static_cast<int>(i) == swapIdxA && swapIdxB >= 0) {
        float startXPos = getSlotX(swapIdxB);
        float endXPos = slotX;
        cardX = startXPos + (endXPos - startXPos) * easeT;
        float arc = std::sin(easeT * 3.14159265f) * 20.0f;
        cardY -= arc; // Lift in arc above other cards
      } else if (static_cast<int>(i) == swapIdxB && swapIdxA >= 0) {
        float startXPos = getSlotX(swapIdxA);
        float endXPos = slotX;
        cardX = startXPos + (endXPos - startXPos) * easeT;
        float arc = std::sin(easeT * 3.14159265f) * 8.0f;
        cardY += arc; // Dip slightly under
      }

      // Mutation state
      bool isMutated = false;
      for (const auto &mut : step.delta.array_mutations) {
        if (mut.variable_name == arrName && mut.index == i) {
          isMutated = true;
          break;
        }
      }

      // Pointer state
      bool isPointed = false;
      for (const auto &it : iterators) {
        if (it.value == static_cast<int>(i)) {
          isPointed = true;
          break;
        }
      }
      if (ptr_j >= 0 && static_cast<int>(i) == (ptr_j + 1)) {
        isPointed = true;
      }

      // Pulsing highlight for mutations and comparisons
      float timePulse =
          0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 5.0f);

      ImU32 cardBg = IM_COL32(30, 41, 59, 230);       // Slate 800
      ImU32 cardBorder = IM_COL32(56, 189, 248, 200); // Sky border
      if (isMutated) {
        int r = 244 + static_cast<int>(11 * timePulse);
        cardBg = IM_COL32(r, 63, 94, 240); // Rose pulse
        cardBorder = IM_COL32(251, 113, 133, 255);
      } else if (isPointed) {
        cardBg = IM_COL32(245, 158, 11, 230); // Amber
        cardBorder = IM_COL32(252, 211, 77, 255);
      }

      // Render rounded bar card
      drawList->AddRectFilled(ImVec2(cardX, cardY),
                              ImVec2(cardX + cardWidth, baseY), cardBg, 8.0f);
      drawList->AddRect(ImVec2(cardX, cardY), ImVec2(cardX + cardWidth, baseY),
                        cardBorder, 8.0f, 0, 2.0f);

      // Render bold centered value
      std::string valStr = arrVal->elements[i].raw_value;
      ImVec2 textSize = ImGui::CalcTextSize(valStr.c_str());
      float textX = cardX + (cardWidth - textSize.x) * 0.5f;
      float textY = cardY + (cardHeight - textSize.y) * 0.5f;
      drawList->AddText(ImVec2(textX, textY), IM_COL32(255, 255, 255, 255),
                        valStr.c_str());

      // Index chip below card
      std::string idxStr = "[" + std::to_string(i) + "]";
      ImVec2 idxSize = ImGui::CalcTextSize(idxStr.c_str());
      float idxX = cardX + (cardWidth - idxSize.x) * 0.5f;
      drawList->AddText(ImVec2(idxX, baseY + 8), IM_COL32(148, 163, 184, 255),
                        idxStr.c_str());

      // Render pointer badges
      float badgeY = baseY + 28;
      for (size_t iterIdx = 0; iterIdx < iterators.size(); ++iterIdx) {
        if (iterators[iterIdx].value != static_cast<int>(i))
          continue;

        ImU32 color = iterColors[iterIdx % numIterColors];
        std::string label = "ptr " + iterators[iterIdx].name;

        // Arrow
        drawList->AddTriangleFilled(
            ImVec2(cardX + cardWidth * 0.5f, badgeY),
            ImVec2(cardX + cardWidth * 0.5f - 6, badgeY + 8),
            ImVec2(cardX + cardWidth * 0.5f + 6, badgeY + 8), color);

        // Pill
        ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
        float pillW = labelSize.x + 14.0f;
        ImVec2 pillMin(cardX + cardWidth * 0.5f - pillW * 0.5f, badgeY + 10);
        ImVec2 pillMax(pillMin.x + pillW, pillMin.y + 20);
        drawList->AddRectFilled(pillMin, pillMax,
                                (color & 0x00FFFFFF) | 0xDC000000, 10.0f);
        drawList->AddText(ImVec2(pillMin.x + 7, pillMin.y + 2),
                          IM_COL32(255, 255, 255, 255), label.c_str());

        badgeY += 32;
      }

      // Pointer badge for comparison partner j+1
      if (ptr_j >= 0 && static_cast<int>(i) == (ptr_j + 1)) {
        ImU32 color = IM_COL32(245, 158, 11, 255);
        std::string label = "ptr j+1";

        drawList->AddTriangleFilled(
            ImVec2(cardX + cardWidth * 0.5f, badgeY),
            ImVec2(cardX + cardWidth * 0.5f - 6, badgeY + 8),
            ImVec2(cardX + cardWidth * 0.5f + 6, badgeY + 8), color);

        ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
        float pillW = labelSize.x + 14.0f;
        ImVec2 pillMin(cardX + cardWidth * 0.5f - pillW * 0.5f, badgeY + 10);
        ImVec2 pillMax(pillMin.x + pillW, pillMin.y + 20);
        drawList->AddRectFilled(pillMin, pillMax,
                                (color & 0x00FFFFFF) | 0xDC000000, 10.0f);
        drawList->AddText(ImVec2(pillMin.x + 7, pillMin.y + 2),
                          IM_COL32(255, 255, 255, 255), label.c_str());
      }
    }

    // Comparison connector bridge
    if (ptr_j >= 0 && (size_t)(ptr_j + 1) < n) {
      float x1 = getSlotX(ptr_j) + cardWidth * 0.5f;
      float x2 = getSlotX(ptr_j + 1) + cardWidth * 0.5f;
      float bridgeY = baseY - 120.0f;
      drawList->AddLine(ImVec2(x1, bridgeY), ImVec2(x2, bridgeY),
                        IM_COL32(245, 158, 11, 255), 2.0f);
      drawList->AddCircleFilled(ImVec2(x1, bridgeY), 4.0f,
                                IM_COL32(245, 158, 11, 255));
      drawList->AddCircleFilled(ImVec2(x2, bridgeY), 4.0f,
                                IM_COL32(245, 158, 11, 255));
      drawList->AddText(ImVec2((x1 + x2) * 0.5f - 4, bridgeY - 18),
                        IM_COL32(245, 158, 11, 255), "⟷");
    } else if (iterators.size() >= 2) {
      int a = iterators[0].value;
      int b = iterators[1].value;
      if (a != b && (size_t)a < n && (size_t)b < n) {
        float x1 = getSlotX(a) + cardWidth * 0.5f;
        float x2 = getSlotX(b) + cardWidth * 0.5f;
        float bridgeY = baseY - 120.0f;
        drawList->AddLine(ImVec2(x1, bridgeY), ImVec2(x2, bridgeY),
                          iterColors[0], 2.0f);
        drawList->AddCircleFilled(ImVec2(x1, bridgeY), 4.0f, iterColors[0]);
        drawList->AddCircleFilled(ImVec2(x2, bridgeY), 4.0f, iterColors[0]);
        drawList->AddText(ImVec2((x1 + x2) * 0.5f - 4, bridgeY - 18),
                          iterColors[0], "⟷");
      }
    }
  }

  ImGui::EndChild();
}

void UIOverlay::renderStdoutPanel(const StepRecord &step, float height) {
  (void)step;
  ImGui::BeginChild("StdoutCard", ImVec2(0, height), true);

  ImGui::TextColored(ImVec4(0.38f, 0.75f, 0.98f, 1.0f),
                     "🖥 TERMINAL OUTPUT (STDOUT)");
  ImGui::Separator();

  ImGui::BeginChild("StdoutContent", ImVec2(0, 0), false,
                    ImGuiWindowFlags_HorizontalScrollbar);
  if (m_monoFont)
    ImGui::PushFont(m_monoFont);

  if (m_cumulativeStdout.empty()) {
    ImGui::TextDisabled("$ (No console output recorded yet)");
  } else {
    ImGui::TextColored(ImVec4(0.9f, 0.95f, 0.9f, 1.0f), "%s",
                       m_cumulativeStdout.c_str());
    ImGui::SetScrollHereY(1.0f);
  }

  if (m_monoFont)
    ImGui::PopFont();
  ImGui::EndChild();
  ImGui::EndChild();
}

void UIOverlay::renderBottomMediaDock(UIPlaybackState &state) {
  ImGui::BeginChild("MediaDock", ImVec2(0, 72), true,
                    ImGuiWindowFlags_NoScrollbar);

  // Keyboard shortcut listeners
  if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
    state.is_paused = !state.is_paused;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
    state.step_forward = true;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
    state.step_backward = true;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_R)) {
    state.restart = true;
  }

  // Controls Cluster
  float centerOffset = (ImGui::GetWindowWidth() - 530.0f) * 0.5f;
  if (centerOffset > 10.0f)
    ImGui::SetCursorPosX(centerOffset);

  if (ImGui::Button("⏮ Restart [R]")) {
    state.restart = true;
  }
  ImGui::SameLine();

  if (ImGui::Button("◀ Prev [←]")) {
    state.step_backward = true;
  }
  ImGui::SameLine();

  // Play / Pause prominent button
  if (state.is_paused) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.73f, 0.51f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.10f, 0.85f, 0.60f, 1.0f));
    if (ImGui::Button("▶  PLAY [Space]")) {
      state.is_paused = false;
    }
    ImGui::PopStyleColor(2);
  } else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.96f, 0.62f, 0.04f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(1.00f, 0.72f, 0.15f, 1.0f));
    if (ImGui::Button("⏸ PAUSE [Space]")) {
      state.is_paused = true;
    }
    ImGui::PopStyleColor(2);
  }
  ImGui::SameLine();

  if (ImGui::Button("Next [→] ▶")) {
    state.step_forward = true;
  }
  ImGui::SameLine(ImGui::GetWindowWidth() - 175.0f);

  // Speed selector
  ImGui::TextDisabled("Speed:");
  ImGui::SameLine();
  if (ImGui::Button("0.5x"))
    state.step_duration = 1.2f;
  ImGui::SameLine();
  if (ImGui::Button("1.0x"))
    state.step_duration = 0.8f;
  ImGui::SameLine();
  if (ImGui::Button("2.0x"))
    state.step_duration = 0.4f;

  // Scrubbable Timeline Slider
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
  int sliderVal = static_cast<int>(state.current_step);
  int maxSteps =
      state.total_steps > 0 ? static_cast<int>(state.total_steps - 1) : 0;

  ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
  if (ImGui::SliderInt("##TimelineScrubber", &sliderVal, 0, maxSteps,
                       "Step %d / %d")) {
    state.scrubbed = true;
    state.scrub_target = static_cast<size_t>(sliderVal);
  }
  ImGui::PopItemWidth();

  ImGui::EndChild();
}

void UIOverlay::renderUI(const ExecutionTrace &trace, UIPlaybackState &state) {
  state.total_steps = trace.steps.size();

  // Accumulate console output up to the active step
  m_cumulativeStdout.clear();
  for (size_t i = 0; i <= state.current_step && i < trace.steps.size(); ++i) {
    if (!trace.steps[i].stdout_chunk.empty()) {
      m_cumulativeStdout += trace.steps[i].stdout_chunk;
    }
  }

  // Fullscreen container window
  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);

  ImGuiWindowFlags windowFlags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::Begin("CRIXVisualizerRoot", nullptr, windowFlags);

  const StepRecord emptyStep{};
  const StepRecord &currentStep =
      trace.steps.empty() ? emptyStep : trace.steps[state.current_step];

  // 1. Top Navbar
  renderTopNavbar(currentStep, state);

  // 2. Main 2-Column Grid Layout
  float totalHeight =
      ImGui::GetContentRegionAvail().y - 82.0f; // Leave space for bottom dock
  float leftWidth = ImGui::GetContentRegionAvail().x * 0.42f;
  float rightWidth = ImGui::GetContentRegionAvail().x - leftWidth - 12.0f;

  // Left Column: Code View + Variables
  ImGui::BeginChild("LeftColumn", ImVec2(leftWidth, totalHeight), false);
  float codeHeight = totalHeight * 0.58f;
  float varsHeight = totalHeight - codeHeight - 8.0f;
  renderCodeViewCard(currentStep.line, codeHeight);
  renderVariablesCard(currentStep, varsHeight, state.animation_progress);
  ImGui::EndChild();

  ImGui::SameLine();

  // Right Column: Step Narration + Canvas + Stdout
  float narrationHeight = 68.0f;
  float stdoutHeight = 110.0f;
  float canvasHeight = totalHeight - narrationHeight - stdoutHeight - 16.0f;
  if (canvasHeight < 150.0f)
    canvasHeight = 150.0f;

  ImGui::BeginChild("RightColumn", ImVec2(rightWidth, totalHeight), false);
  renderStepNarrationCard(currentStep, state.current_step);
  renderArrayCanvasCard(trace, state, canvasHeight);
  renderStdoutPanel(currentStep, stdoutHeight);
  ImGui::EndChild();

  // 3. Bottom Media Dock
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
