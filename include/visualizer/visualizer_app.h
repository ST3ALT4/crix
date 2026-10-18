#ifndef VISUALIZER_APP_H
#define VISUALIZER_APP_H

#include "trace_model.h"
#include "visualizer/vk_context.h"
#include "visualizer/vk_pipeline.h"
#include "visualizer/scene_renderer.h"

#include <memory>
#include <vector>

#include "visualizer/ui_overlay.h"

class VisualizerApp {
public:
  VisualizerApp(const ExecutionTrace &trace);
  ~VisualizerApp();

  void run();

private:
  void initVulkan();
  void createFramebuffers();
  void createCommandBuffers();
  void createSyncObjects();
  void drawFrame();

  ExecutionTrace m_trace;
  UIPlaybackState m_playbackState;

  std::unique_ptr<VulkanContext> m_context;
  std::unique_ptr<VulkanPipeline> m_pipeline;
  std::unique_ptr<SceneRenderer> m_renderer;
  std::unique_ptr<UIOverlay> m_uiOverlay;

  std::vector<vk::Framebuffer> m_swapChainFramebuffers;
  std::vector<vk::CommandBuffer> m_commandBuffers;

  vk::Semaphore m_imageAvailableSemaphore;
  vk::Semaphore m_renderFinishedSemaphore;
  vk::Fence m_inFlightFence;
};

void run_visualizer(const ExecutionTrace &trace);

#endif // VISUALIZER_APP_H
