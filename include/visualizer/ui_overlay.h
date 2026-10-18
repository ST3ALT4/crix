#ifndef UI_OVERLAY_H
#define UI_OVERLAY_H

#include "trace_model.h"
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
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
};

class UIOverlay {
public:
  UIOverlay(GLFWwindow *window, vk::Instance instance, vk::PhysicalDevice physicalDevice,
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

  // Sub-components
  void renderTopNavbar(const StepRecord &step, const UIPlaybackState &state);
  void renderCodeViewCard(int currentLine, float height);
  void renderVariablesCard(const StepRecord &step, float height, float animProgress);
  void renderStepNarrationCard(const StepRecord &step, size_t stepIndex);
  void renderArrayCanvasCard(const ExecutionTrace &trace, const UIPlaybackState &state, float height);
  void renderStdoutPanel(const StepRecord &step, float height);
  void renderBottomMediaDock(UIPlaybackState &state);

  std::string generateNarrationText(const StepRecord &step);

  GLFWwindow *m_window;
  vk::Device m_device;
  vk::DescriptorPool m_imguiDescriptorPool;

  std::string m_sourcePath;
  std::vector<std::string> m_sourceLines;
  int m_lastActiveLine = -1;
  ImFont *m_monoFont = nullptr;
  std::string m_cumulativeStdout;
};

#endif // UI_OVERLAY_H
