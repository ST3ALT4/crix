#ifndef UI_OVERLAY_H
#define UI_OVERLAY_H

#include "trace_model.h"
#include "visualizer/anim_engine.h"
#include "visualizer/trace_visual_adapter.h"
#include <vulkan/vulkan.hpp>
#include <SDL3/SDL.h>
#include <string>
#include <vector>

struct ImFont;

struct UIPlaybackState {
  size_t current_step = 0;
  size_t total_steps = 0;
  bool is_paused = true;
  float step_duration = 0.8f; // Seconds per step
  float timer = 0.0f;

  float animation_progress = 1.0f; // 0.0f to 1.0f within active step transition
  float transition_timer = 0.0f;
  float transition_duration = 0.35f;
  bool in_transition = false;

  bool step_forward = false;
  bool step_backward = false;
  bool restart = false;
  bool scrubbed = false;
  size_t scrub_target = 0;

  // Name of the observed variable/data structure selected in drop sidebar
  std::string selected_data_structure;
};

class UIOverlay {
public:
  UIOverlay(SDL_Window *window, vk::Instance instance, vk::PhysicalDevice physicalDevice,
            vk::Device device, uint32_t queueFamily, vk::Queue queue,
            vk::RenderPass renderPass, uint32_t imageCount,
            const std::string &sourceFilePath);
  ~UIOverlay();

  void newFrame();
  void renderUI(const ExecutionTrace &trace, UIPlaybackState &state);
  void drawFrame(vk::CommandBuffer cmd);
  void onResize(uint32_t width, uint32_t height);

private:
  void applyModernWebTheme();
  void loadSourceFile(const std::string &path);

  // Top navigation bar
  void renderTopNavbar(const StepRecord &step, const UIPlaybackState &state);

  // The 3 Panes
  void renderCodeExecutingPane(int currentLine, float width, float height);
  void renderStackTracePane(const StepRecord &step, float width, float height);
  void renderAnimationCanvasPane(const ExecutionTrace &trace, const UIPlaybackState &state, float width, float height);

  // Drop Sidebar for data structure selection
  void renderDataStructureDropSidebar(const StepRecord &step, const ExecutionTrace &trace, UIPlaybackState &state);

  // Bottom playback media player dock
  void renderBottomMediaDock(UIPlaybackState &state);

  std::string generateNarrationText(const StepRecord &step);

  SDL_Window *m_window;
  vk::Device m_device;
  vk::DescriptorPool m_imguiDescriptorPool;

  std::string m_sourcePath;
  std::vector<std::string> m_sourceLines;
  int m_lastActiveLine = -1;
  ImFont *m_monoFont = nullptr;
  std::string m_cumulativeStdout;

  // Modular animation engine
  AnimationEngine m_animEngine;

  // Drop sidebar toggle state
  bool m_sidebarOpen = false;
};

#endif // UI_OVERLAY_H
