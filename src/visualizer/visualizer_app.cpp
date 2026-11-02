#include "visualizer/visualizer_app.h"
#include <iostream>
#include <chrono>

VisualizerApp::VisualizerApp(const ExecutionTrace &trace)
    : m_trace(trace) {
  initVulkan();
}

VisualizerApp::~VisualizerApp() {
  auto device = m_context->getDevice();
  device.waitIdle();

  if (m_inFlightFence) device.destroyFence(m_inFlightFence);
  if (m_renderFinishedSemaphore) device.destroySemaphore(m_renderFinishedSemaphore);
  if (m_imageAvailableSemaphore) device.destroySemaphore(m_imageAvailableSemaphore);

  for (auto fb : m_swapChainFramebuffers) {
    device.destroyFramebuffer(fb);
  }
}

void VisualizerApp::initVulkan() {
  m_context = std::make_unique<VulkanContext>(1280, 720, "CRIX - Code Runtime & Input eXplorer (SDL3 + Vulkan + Slang)");

  m_pipeline = std::make_unique<VulkanPipeline>(
      m_context->getDevice(),
      m_context->getSwapChainExtent(),
      m_context->getSwapChainImageFormat(),
      "shaders/bar_visualizer.vert.spv",
      "shaders/bar_visualizer.frag.spv"
  );

  m_renderer = std::make_unique<SceneRenderer>(*m_context, *m_pipeline);

  createFramebuffers();
  createCommandBuffers();
  createSyncObjects();

  m_uiOverlay = std::make_unique<UIOverlay>(
      m_context->getWindow(),
      m_context->getInstance(),
      m_context->getPhysicalDevice(),
      m_context->getDevice(),
      m_context->getGraphicsQueueFamily(),
      m_context->getGraphicsQueue(),
      m_pipeline->getRenderPass(),
      static_cast<uint32_t>(m_swapChainFramebuffers.size()),
      m_trace.source_file
  );

  if (!m_trace.steps.empty()) {
    m_renderer->updateDataFromStep(m_trace.steps[0], 0.0f, m_playbackState.selected_data_structure);
  }
}

void VisualizerApp::createFramebuffers() {
  auto device = m_context->getDevice();
  const auto &imageViews = m_context->getSwapChainImageViews();
  auto extent = m_context->getSwapChainExtent();

  m_swapChainFramebuffers.resize(imageViews.size());

  for (size_t i = 0; i < imageViews.size(); i++) {
    vk::ImageView attachments[] = {imageViews[i]};

    vk::FramebufferCreateInfo framebufferInfo(
        {},
        m_pipeline->getRenderPass(),
        1, attachments,
        extent.width,
        extent.height,
        1
    );

    m_swapChainFramebuffers[i] = device.createFramebuffer(framebufferInfo);
  }
}

void VisualizerApp::createCommandBuffers() {
  auto device = m_context->getDevice();
  auto pool = m_context->getCommandPool();

  vk::CommandBufferAllocateInfo allocInfo(
      pool,
      vk::CommandBufferLevel::ePrimary,
      static_cast<uint32_t>(m_swapChainFramebuffers.size())
  );

  m_commandBuffers = device.allocateCommandBuffers(allocInfo);
}

void VisualizerApp::createSyncObjects() {
  auto device = m_context->getDevice();

  vk::SemaphoreCreateInfo semaphoreInfo{};
  vk::FenceCreateInfo fenceInfo(vk::FenceCreateFlagBits::eSignaled);

  m_imageAvailableSemaphore = device.createSemaphore(semaphoreInfo);
  m_renderFinishedSemaphore = device.createSemaphore(semaphoreInfo);
  m_inFlightFence = device.createFence(fenceInfo);
}

void VisualizerApp::recreateSwapChain() {
  auto device = m_context->getDevice();
  device.waitIdle();

  for (auto fb : m_swapChainFramebuffers) {
    device.destroyFramebuffer(fb);
  }
  m_swapChainFramebuffers.clear();

  m_context->recreateSwapChain();
  createFramebuffers();
}

void VisualizerApp::drawFrame() {
  auto device = m_context->getDevice();

  // Wait for previous frame
  (void)device.waitForFences(1, &m_inFlightFence, VK_TRUE, UINT64_MAX);
  (void)device.resetFences(1, &m_inFlightFence);

  uint32_t imageIndex;
  try {
    auto result = device.acquireNextImageKHR(
        m_context->getSwapChain(),
        UINT64_MAX,
        m_imageAvailableSemaphore,
        nullptr
    );
    imageIndex = result.value;
  } catch (const vk::OutOfDateKHRError &) {
    recreateSwapChain();
    return;
  }

  auto cmd = m_commandBuffers[imageIndex];
  cmd.reset();

  vk::CommandBufferBeginInfo beginInfo{};
  cmd.begin(beginInfo);

  vk::ClearValue clearColor(vk::ClearColorValue(std::array<float, 4>{0.04f, 0.06f, 0.09f, 1.0f}));
  vk::RenderPassBeginInfo renderPassInfo(
      m_pipeline->getRenderPass(),
      m_swapChainFramebuffers[imageIndex],
      vk::Rect2D({0, 0}, m_context->getSwapChainExtent()),
      1, &clearColor
  );

  cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
  m_renderer->render(imageIndex, cmd);
  if (m_uiOverlay) {
    m_uiOverlay->drawFrame(cmd);
  }
  cmd.endRenderPass();
  cmd.end();

  vk::Semaphore waitSemaphores[] = {m_imageAvailableSemaphore};
  vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
  vk::Semaphore signalSemaphores[] = {m_renderFinishedSemaphore};

  vk::SubmitInfo submitInfo(
      1, waitSemaphores, waitStages,
      1, &cmd,
      1, signalSemaphores
  );

  m_context->getGraphicsQueue().submit(submitInfo, m_inFlightFence);

  vk::SwapchainKHR swapChains[] = {m_context->getSwapChain()};
  vk::PresentInfoKHR presentInfo(
      1, signalSemaphores,
      1, swapChains,
      &imageIndex
  );

  try {
    (void)m_context->getPresentQueue().presentKHR(presentInfo);
  } catch (const vk::OutOfDateKHRError &) {
    recreateSwapChain();
  }
}

void VisualizerApp::run() {
  auto lastTime = std::chrono::high_resolution_clock::now();

  std::cout << "[SDL3 + Vulkan Visualizer] Modern Web-Style UI active. Press ESC to exit, Space to toggle Play/Pause, Left/Right arrow to step.\n";

  while (!m_context->shouldClose()) {
    m_context->pollEvents();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
    lastTime = currentTime;

    // Start UI Frame
    if (m_uiOverlay) {
      m_uiOverlay->newFrame();
      m_uiOverlay->renderUI(m_trace, m_playbackState);
    }

    // Process playback commands from UI
    if (m_playbackState.restart) {
      m_playbackState.current_step = 0;
      m_playbackState.restart = false;
      m_playbackState.timer = 0.0f;
      m_playbackState.animation_progress = 1.0f;
      m_playbackState.in_transition = false;
      if (!m_trace.steps.empty()) {
        m_renderer->updateDataFromStep(m_trace.steps[0], 1.0f, m_playbackState.selected_data_structure);
      }
    } else if (m_playbackState.step_forward) {
      if (m_playbackState.current_step + 1 < m_trace.steps.size()) {
        m_playbackState.current_step++;
        m_playbackState.transition_timer = 0.0f;
        m_playbackState.in_transition = true;
        m_playbackState.animation_progress = 0.0f;
        m_renderer->updateDataFromStep(m_trace.steps[m_playbackState.current_step], 0.0f, m_playbackState.selected_data_structure);
      }
      m_playbackState.step_forward = false;
    } else if (m_playbackState.step_backward) {
      if (m_playbackState.current_step > 0) {
        m_playbackState.current_step--;
        m_playbackState.transition_timer = 0.0f;
        m_playbackState.in_transition = true;
        m_playbackState.animation_progress = 0.0f;
        m_renderer->updateDataFromStep(m_trace.steps[m_playbackState.current_step], 0.0f, m_playbackState.selected_data_structure);
      }
      m_playbackState.step_backward = false;
    } else if (m_playbackState.scrubbed) {
      if (!m_trace.steps.empty()) {
        m_playbackState.current_step = std::min(m_playbackState.scrub_target, m_trace.steps.size() - 1);
        m_playbackState.animation_progress = 1.0f;
        m_playbackState.in_transition = false;
        m_renderer->updateDataFromStep(m_trace.steps[m_playbackState.current_step], 1.0f, m_playbackState.selected_data_structure);
      }
      m_playbackState.scrubbed = false;
    } else if (!m_playbackState.is_paused) {
      m_playbackState.timer += dt;
      m_playbackState.animation_progress = m_playbackState.step_duration > 0.0f
          ? std::clamp(m_playbackState.timer / m_playbackState.step_duration, 0.0f, 1.0f)
          : 1.0f;
      m_renderer->updateDataFromStep(m_trace.steps[m_playbackState.current_step], m_playbackState.animation_progress, m_playbackState.selected_data_structure);

      if (m_playbackState.timer >= m_playbackState.step_duration) {
        m_playbackState.timer = 0.0f;
        if (m_playbackState.current_step + 1 < m_trace.steps.size()) {
          m_playbackState.current_step++;
          m_playbackState.animation_progress = 0.0f;
          m_renderer->updateDataFromStep(m_trace.steps[m_playbackState.current_step], 0.0f, m_playbackState.selected_data_structure);
        } else {
          m_playbackState.is_paused = true; // Auto-pause at end of trace
          m_playbackState.animation_progress = 1.0f;
        }
      }
    } else if (m_playbackState.in_transition) {
      m_playbackState.transition_timer += dt;
      m_playbackState.animation_progress = std::clamp(
          m_playbackState.transition_timer / m_playbackState.transition_duration, 0.0f, 1.0f);
      m_renderer->updateDataFromStep(m_trace.steps[m_playbackState.current_step], m_playbackState.animation_progress, m_playbackState.selected_data_structure);

      if (m_playbackState.transition_timer >= m_playbackState.transition_duration) {
        m_playbackState.in_transition = false;
        m_playbackState.animation_progress = 1.0f;
      }
    }

    drawFrame();
  }
}

void run_visualizer(const ExecutionTrace &trace) {
  VisualizerApp app(trace);
  app.run();
}
