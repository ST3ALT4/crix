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

  // VS Code / Professional Creative Tool Geometry (tight, utilitarian, no bloated radii)
  style.WindowRounding    = 2.0f;
  style.ChildRounding     = 2.0f;
  style.FrameRounding     = 2.0f;
  style.PopupRounding     = 2.0f;
  style.ScrollbarRounding = 2.0f;
  style.GrabRounding      = 2.0f;
  style.TabRounding       = 2.0f;

  style.WindowBorderSize  = 1.0f;
  style.ChildBorderSize   = 1.0f;
  style.FrameBorderSize   = 1.0f;

  style.WindowPadding     = ImVec2(8.0f, 8.0f);
  style.FramePadding      = ImVec2(6.0f, 3.0f);
  style.ItemSpacing       = ImVec2(6.0f, 4.0f);
  style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);

  // VS Code Dark Modern Neutral Palette (#1e1e1e base, #252526 sidebar, #333333 frames)
  colors[ImGuiCol_Text]                 = ImVec4(0.85f, 0.85f, 0.85f, 1.00f); // #d4d4d4
  colors[ImGuiCol_TextDisabled]         = ImVec4(0.52f, 0.52f, 0.52f, 1.00f); // #858585
  colors[ImGuiCol_WindowBg]             = ImVec4(0.118f, 0.118f, 0.118f, 1.00f); // #1e1e1e (Editor base)
  colors[ImGuiCol_ChildBg]              = ImVec4(0.145f, 0.145f, 0.149f, 1.00f); // #252526 (Sidebar / panels)
  colors[ImGuiCol_PopupBg]              = ImVec4(0.176f, 0.176f, 0.176f, 1.00f); // #2d2d2d (Flyouts / palettes)
  colors[ImGuiCol_Border]               = ImVec4(0.235f, 0.235f, 0.235f, 1.00f); // #3c3c3c (Subtle divider)
  colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_FrameBg]              = ImVec4(0.200f, 0.200f, 0.200f, 1.00f); // #333333 (Inputs & chips)
  colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.240f, 0.240f, 0.240f, 1.00f);
  colors[ImGuiCol_FrameBgActive]        = ImVec4(0.000f, 0.480f, 0.800f, 0.80f); // #007acc (VS Code focus blue)
  colors[ImGuiCol_TitleBg]              = ImVec4(0.120f, 0.120f, 0.120f, 1.00f);
  colors[ImGuiCol_TitleBgActive]        = ImVec4(0.145f, 0.145f, 0.149f, 1.00f);
  colors[ImGuiCol_MenuBarBg]            = ImVec4(0.180f, 0.180f, 0.180f, 1.00f);
  colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.118f, 0.118f, 0.118f, 1.00f);
  colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.260f, 0.260f, 0.260f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.350f, 0.350f, 0.350f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.450f, 0.450f, 0.450f, 1.00f);

  // VS Code Accent Colors: Electric Cyan / Focus Blue (#007acc & #00d1ff)
  colors[ImGuiCol_CheckMark]            = ImVec4(0.00f, 0.82f, 1.00f, 1.00f); // Electric Cyan
  colors[ImGuiCol_SliderGrab]           = ImVec4(0.00f, 0.48f, 0.80f, 1.00f); // #007acc
  colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.00f, 0.82f, 1.00f, 1.00f);
  colors[ImGuiCol_Button]               = ImVec4(0.180f, 0.180f, 0.180f, 1.00f);
  colors[ImGuiCol_ButtonHovered]        = ImVec4(0.000f, 0.480f, 0.800f, 0.70f); // #007acc hover
  colors[ImGuiCol_ButtonActive]         = ImVec4(0.000f, 0.480f, 0.800f, 1.00f);
  colors[ImGuiCol_Header]               = ImVec4(0.165f, 0.176f, 0.180f, 1.00f); // #2a2d2e
  colors[ImGuiCol_HeaderHovered]        = ImVec4(0.216f, 0.216f, 0.239f, 1.00f); // #37373d
  colors[ImGuiCol_HeaderActive]         = ImVec4(0.035f, 0.278f, 0.443f, 1.00f); // #094771 (VS Code active item)
  colors[ImGuiCol_Separator]            = ImVec4(0.235f, 0.235f, 0.235f, 1.00f); // #3c3c3c
  colors[ImGuiCol_Tab]                  = ImVec4(0.145f, 0.145f, 0.149f, 1.00f);
  colors[ImGuiCol_TabHovered]           = ImVec4(0.216f, 0.216f, 0.239f, 1.00f);
  colors[ImGuiCol_TabActive]            = ImVec4(0.118f, 0.118f, 0.118f, 1.00f);
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
  ImGui::BeginChild("TopNavbar", ImVec2(0, 38), true, ImGuiWindowFlags_NoScrollbar);

  // VS Code Title Brand Badge
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.48f, 0.80f, 1.0f)); // #007acc
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
  ImGui::Button("CRIX");
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
  ImGui::SameLine();

  // Breadcrumbs: workspace > file > function() : Ln
  ImGui::TextDisabled("workspace >");
  ImGui::SameLine();
  std::string fileName = m_sourcePath;
  size_t lastSlash = fileName.find_last_of("/\\");
  if (lastSlash != std::string::npos) fileName = fileName.substr(lastSlash + 1);
  ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "%s", fileName.c_str());
  ImGui::SameLine();
  ImGui::TextDisabled(">");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.00f, 0.82f, 1.00f, 1.0f), "%s()", step.function_name.c_str());
  ImGui::SameLine();
  ImGui::TextDisabled(": Ln %d", step.line);

  // VS Code Command Center Button (centered)
  float centerBoxW = 320.0f;
  float centerBoxX = (ImGui::GetWindowWidth() - centerBoxW) * 0.5f;
  if (centerBoxX > 320.0f) {
    ImGui::SameLine(centerBoxX);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.26f, 0.26f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 3.0f));
    if (ImGui::Button("Search commands, jump to step... (Ctrl+K)", ImVec2(centerBoxW, 24))) {
      m_cmdPaletteOpen = true;
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
  }

  // Right-aligned Controls
  float rightAreaX = ImGui::GetWindowWidth() - 360.0f;
  if (rightAreaX > 540.0f) {
    ImGui::SameLine(rightAreaX);
  } else {
    ImGui::SameLine();
  }

  // Drop Sidebar Toggle Button
  std::string targetLabel = state.selected_data_structure.empty()
                                ? "Data Structures"
                                : ("Target: " + state.selected_data_structure);
  if (m_sidebarOpen) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.48f, 0.80f, 1.0f)); // Active blue
  } else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
  }
  if (ImGui::Button(targetLabel.c_str(), ImVec2(0, 24))) {
    m_sidebarOpen = !m_sidebarOpen;
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();

  // Play/Pause Status Pill
  if (state.is_paused) {
    ImGui::TextColored(ImVec4(0.96f, 0.62f, 0.04f, 1.0f), "PAUSED");
  } else {
    ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.98f, 1.0f), "STEPPING");
  }
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.70f, 0.70f, 0.70f, 1.0f), "Step %zu/%zu",
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
// PANE 1: Code Executing (VS Code Monaco Look)
// ─────────────────────────────────────────────────────────────────────────────
void UIOverlay::renderCodeExecutingPane(int currentLine, float width, float height) {
  ImGui::BeginChild("PaneCodeExecuting", ImVec2(width, height), true);

  // Tab Header Bar
  std::string fileName = m_sourcePath;
  size_t lastSlash = fileName.find_last_of("/\\");
  if (lastSlash != std::string::npos) fileName = fileName.substr(lastSlash + 1);

  ImGui::TextColored(ImVec4(0.00f, 0.82f, 1.00f, 1.0f), "%s", fileName.c_str());
  ImGui::SameLine();
  ImGui::TextDisabled("| Ln %d", currentLine);
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

      // VS Code style active line highlight (dark blue-gray row + cyan left edge)
      ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, IM_COL32(35, 45, 60, 255), 2.0f);
      ImGui::GetWindowDrawList()->AddRectFilled(p_min, ImVec2(p_min.x + 3, p_max.y),
                                                IM_COL32(0, 209, 255, 255), 1.0f);

      // Auto-scroll on step change
      if (m_lastActiveLine != currentLine) {
        ImGui::SetScrollHereY(0.4f);
        m_lastActiveLine = currentLine;
      }
    }

    if (isActive) {
      ImGui::TextColored(ImVec4(0.00f, 0.82f, 1.00f, 1.0f), "%3d ▶ ", lineNum);
    } else {
      ImGui::TextDisabled("%3d │ ", lineNum);
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
// PANE 2: Stack Trace & Variables (VS Code Run & Debug View)
// ─────────────────────────────────────────────────────────────────────────────
void UIOverlay::renderStackTracePane(const StepRecord &step, float width, float height) {
  ImGui::BeginChild("PaneStackTrace", ImVec2(width, height), true);

  ImGui::TextColored(ImVec4(0.00f, 0.82f, 1.00f, 1.0f), "RUN & DEBUG");
  ImGui::SameLine();
  ImGui::TextDisabled("| PID Active");
  ImGui::Separator();

  ImGui::BeginChild("DebugSidebarScroll", ImVec2(0, 0), false);

  // 1. CALL STACK SECTION
  if (ImGui::CollapsingHeader("CALL STACK", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (step.call_stack.empty()) {
      ImGui::TextDisabled("  No active stack frames.");
    } else {
      for (size_t i = 0; i < step.call_stack.size(); ++i) {
        const auto &frame = step.call_stack[i];
        bool isTop = (i == 0);

        ImGui::PushID(static_cast<int>(i));
        if (isTop) {
          ImGui::TextColored(ImVec4(0.00f, 0.82f, 1.00f, 1.0f), "  ▶ [%zu] %s()", i, frame.name.c_str());
        } else {
          ImGui::TextDisabled("    [%zu] %s()", i, frame.name.c_str());
        }
        std::string loc = "       " + (frame.file.empty() ? step.file : frame.file) + ":" + std::to_string(frame.line);
        ImGui::TextDisabled("%s", loc.c_str());
        ImGui::PopID();
      }
    }
    ImGui::Spacing();
  }

  // 2. VARIABLES SECTION (Inspecting Real State & Deltas)
  if (ImGui::CollapsingHeader("VARIABLES", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (step.variables.empty()) {
      ImGui::TextDisabled("  No variables in local scope.");
    } else {
      // Create quick lookup set of mutated variables in this step
      std::unordered_set<std::string> mutatedSet;
      for (const auto &mv : step.delta.modified_vars) mutatedSet.insert(mv);

      for (const auto &v : step.variables) {
        const std::string &varName = v.first;
        const RuntimeValue &val = v.second;
        bool isMutated = mutatedSet.count(varName) > 0;

        // Symbol Kind Badge if available from LSP
        std::string kindStr = "[VAR]";
        auto kIt = step.var_symbol_kinds.find(varName);
        if (kIt != step.var_symbol_kinds.end()) {
          if (kIt->second == 12) kindStr = "[FN]";
          else if (kIt->second == 25) kindStr = "[PARAM]";
          else if (kIt->second == 22) kindStr = "[STRUCT]";
        }
        if (val.kind == ValueKind::ARRAY) kindStr = "[ARR]";
        else if (val.kind == ValueKind::POINTER) kindStr = "[PTR]";

        if (isMutated) {
          ImGui::TextColored(ImVec4(1.00f, 0.80f, 0.20f, 1.0f), "  * %s %s:", kindStr.c_str(), varName.c_str());
          ImGui::SameLine();
          ImGui::TextColored(ImVec4(1.00f, 0.80f, 0.20f, 1.0f), "%s [MUTATED]", val.to_string().c_str());
        } else {
          ImGui::TextColored(ImVec4(0.70f, 0.70f, 0.70f, 1.0f), "    %s %s:", kindStr.c_str(), varName.c_str());
          ImGui::SameLine();
          ImGui::TextColored(ImVec4(0.90f, 0.90f, 0.90f, 1.0f), "%s", val.to_string().c_str());
        }

        // Show LSP scope if available
        auto sIt = step.var_scope_names.find(varName);
        if (sIt != step.var_scope_names.end() && !sIt->second.empty()) {
          ImGui::TextDisabled("        scope: %s", sIt->second.c_str());
        }
      }
    }
    ImGui::Spacing();
  }

  // 3. LSP & DWARF METADATA
  if (ImGui::CollapsingHeader("DEBUG INFO & SCOPE", ImGuiTreeNodeFlags_None)) {
    ImGui::TextDisabled("  Adapter: lldb-dap");
    ImGui::TextDisabled("  Symbol Provider: clangd (LSP)");
    ImGui::TextDisabled("  DWARF Info: .debug_info Valid");
    ImGui::TextDisabled("  Target Arch: Linux x86_64");
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
// Bottom Playback Media Dock (Blender / Transport Controls)
// ─────────────────────────────────────────────────────────────────────────────
void UIOverlay::renderBottomMediaDock(UIPlaybackState &state) {
  ImGui::BeginChild("BottomMediaDock", ImVec2(0, 42), true, ImGuiWindowFlags_NoScrollbar);

  // Restart Button
  if (ImGui::Button("⏮ [R]", ImVec2(52, 26))) {
    state.restart = true;
  }
  ImGui::SameLine();

  // Step Backward Button
  if (ImGui::Button("◀ [J]", ImVec2(52, 26))) {
    state.step_backward = true;
  }
  ImGui::SameLine();

  // Play / Pause Button
  const char *playPauseLabel = state.is_paused ? "▶ Play [Space]" : "⏸ Pause [Space]";
  if (state.is_paused) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.48f, 0.80f, 1.0f)); // #007acc
  } else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.96f, 0.62f, 0.04f, 1.0f)); // Amber
  }
  if (ImGui::Button(playPauseLabel, ImVec2(120, 26))) {
    state.is_paused = !state.is_paused;
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();

  // Step Forward Button
  if (ImGui::Button("▶ [L]", ImVec2(52, 26))) {
    state.step_forward = true;
  }
  ImGui::SameLine();

  // Speed Selector Buttons
  ImGui::TextDisabled("Speed:");
  ImGui::SameLine();
  const float speeds[] = {1.5f, 0.8f, 0.35f, 0.1f};
  const char *speedLabels[] = {"0.5x", "1.0x", "2.0x", "5.0x"};
  for (int sIdx = 0; sIdx < 4; ++sIdx) {
    bool isCurrent = std::abs(state.step_duration - speeds[sIdx]) < 0.01f;
    if (isCurrent) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.48f, 0.80f, 1.0f));
    } else {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
    }
    if (ImGui::Button(speedLabels[sIdx], ImVec2(42, 24))) {
      state.step_duration = speeds[sIdx];
    }
    ImGui::PopStyleColor();
    ImGui::SameLine();
  }

  // Timeline Scrubber Slider
  ImGui::SameLine();
  float sliderWidth = ImGui::GetContentRegionAvail().x - 10.0f;
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
// VS Code Bottom Status Bar
// ─────────────────────────────────────────────────────────────────────────────
void UIOverlay::renderBottomStatusBar(const StepRecord &step, const UIPlaybackState &state) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.00f, 0.48f, 0.80f, 1.0f)); // VS Code Status Blue (#007acc)
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 3.0f));
  ImGui::BeginChild("BottomStatusBar", ImVec2(0, 24), false, ImGuiWindowFlags_NoScrollbar);

  // Left items
  ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "main*");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.85f, 0.95f, 1.0f, 1.0f), "|");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.30f, 1.00f, 0.60f, 1.0f), "● DAP: lldb-dap");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.85f, 0.95f, 1.0f, 1.0f), "|");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "ELF x86_64");

  // Right items
  float rightStart = ImGui::GetWindowWidth() - 500.0f;
  if (rightStart > 260.0f) {
    ImGui::SameLine(rightStart);
  } else {
    ImGui::SameLine();
  }

  ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Step %zu of %zu",
                     state.total_steps > 0 ? (state.current_step + 1) : 0, state.total_steps);
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.85f, 0.95f, 1.0f, 1.0f), "|");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Ln %d, Col %d", step.line, step.col > 0 ? step.col : 1);
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.85f, 0.95f, 1.0f, 1.0f), "|");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "UTF-8");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.85f, 0.95f, 1.0f, 1.0f), "|");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.00f, 0.90f, 1.00f, 1.0f), "Ctrl+K: Palette");

  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
// Command Palette (VS Code Ctrl+K / Ctrl+Shift+P Modal)
// ─────────────────────────────────────────────────────────────────────────────
void UIOverlay::renderCommandPalette(const StepRecord &step, const ExecutionTrace &trace, UIPlaybackState &state) {
  if (!m_cmdPaletteOpen) return;

  ImGui::OpenPopup("CommandPaletteModal");

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(ImVec2(center.x - 270.0f, center.y - 160.0f), ImGuiCond_Appearing);
  ImGui::SetNextWindowSize(ImVec2(540.0f, 320.0f));

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
  ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.15f, 0.15f, 0.16f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.00f, 0.48f, 0.80f, 1.0f));

  if (ImGui::BeginPopupModal("CommandPaletteModal", &m_cmdPaletteOpen, flags)) {
    // Search input
    ImGui::SetNextItemWidth(-1);
    if (ImGui::IsWindowAppearing()) {
      ImGui::SetKeyboardFocusHere();
    }
    ImGui::InputTextWithHint("##PaletteFilter", "Type a command or filter data structures (Esc to close)...", m_cmdFilter, sizeof(m_cmdFilter));
    ImGui::Separator();

    std::string filterStr = m_cmdFilter;
    for (char &c : filterStr) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    auto matchFilter = [&](const std::string &text) {
      if (filterStr.empty()) return true;
      std::string lowerText = text;
      for (char &c : lowerText) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      return lowerText.find(filterStr) != std::string::npos;
    };

    ImGui::BeginChild("PaletteItems", ImVec2(0, 0), false);

    // Command entries
    if (matchFilter("Step Forward Next Delta (Space / L)")) {
      if (ImGui::Selectable("▶  Trace: Step Forward [Space / L]")) {
        state.step_forward = true;
        m_cmdPaletteOpen = false;
      }
    }
    if (matchFilter("Step Backward Prev Delta (J)")) {
      if (ImGui::Selectable("◀  Trace: Step Backward [J]")) {
        state.step_backward = true;
        m_cmdPaletteOpen = false;
      }
    }
    if (matchFilter("Toggle Play Pause (Space)")) {
      if (ImGui::Selectable(state.is_paused ? "▶  Trace: Play" : "⏸  Trace: Pause")) {
        state.is_paused = !state.is_paused;
        m_cmdPaletteOpen = false;
      }
    }
    if (matchFilter("Reset Execution to main (R)")) {
      if (ImGui::Selectable("⏮  Trace: Reset to Entry Point [R]")) {
        state.restart = true;
        m_cmdPaletteOpen = false;
      }
    }
    if (matchFilter("Toggle Data Structures Sidebar")) {
      if (ImGui::Selectable("▾  View: Toggle Observed Data Structures Sidebar")) {
        m_sidebarOpen = !m_sidebarOpen;
        m_cmdPaletteOpen = false;
      }
    }
    if (matchFilter("Topology: Auto Select from Active Scope")) {
      if (ImGui::Selectable("✦  Topology: Auto Select Active Scope")) {
        state.selected_data_structure.clear();
        m_cmdPaletteOpen = false;
      }
    }
    if (matchFilter("Topology: Call Stack Recursion Tree")) {
      if (ImGui::Selectable("🌳 Topology: Call Stack / Recursion Tree")) {
        state.selected_data_structure = "call_stack";
        m_cmdPaletteOpen = false;
      }
    }

    // Dynamic Variables in scope
    auto candidates = TraceVisualAdapter::discoverDataStructures(step, trace);
    for (const auto &c : candidates) {
      std::string label = "🔍 Observe: " + c.name + " (" + c.type + ")";
      if (matchFilter(label)) {
        if (ImGui::Selectable(label.c_str())) {
          state.selected_data_structure = c.name;
          m_cmdPaletteOpen = false;
        }
      }
    }

    ImGui::EndChild();
    ImGui::EndPopup();
  }

  ImGui::PopStyleColor(2);
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

  // Handle Global Power-User Hotkeys
  ImGuiIO &io = ImGui::GetIO();
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_K)) {
    m_cmdPaletteOpen = !m_cmdPaletteOpen;
  }
  if (!m_cmdPaletteOpen && !io.WantTextInput) {
    if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
      state.is_paused = !state.is_paused;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_J)) {
      state.step_backward = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_L)) {
      state.step_forward = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R)) {
      state.restart = true;
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

  // 2. Three Panes Layout (reserving space for media dock + VS Code status bar)
  float totalHeight = ImGui::GetContentRegionAvail().y - 42.0f - 24.0f - 8.0f;
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

  // Bottom Media Dock (Blender / Transport Controls)
  renderBottomMediaDock(state);

  // VS Code Bottom Status Bar
  renderBottomStatusBar(currentStep, state);

  // Command Palette Modal (Ctrl+K)
  renderCommandPalette(currentStep, trace, state);

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
