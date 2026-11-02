#include "visualizer/vk_context.h"
#include <iostream>
#include <stdexcept>
#include <set>
#include <algorithm>
#include <limits>

static const std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

VulkanContext::VulkanContext(int width, int height, const std::string &title) {
  initWindow(width, height, title);
  initVulkan();
}

VulkanContext::~VulkanContext() {
  cleanupSwapChain();

  if (m_commandPool) m_device.destroyCommandPool(m_commandPool);
  if (m_device) m_device.destroy();
  if (m_surface) m_instance.destroySurfaceKHR(m_surface);
  if (m_instance) m_instance.destroy();

  if (m_window) {
    SDL_DestroyWindow(m_window);
    SDL_Quit();
  }
}

bool VulkanContext::shouldClose() const {
  return m_shouldClose;
}

bool VulkanContext::pollEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    // Forward every event to ImGui first
    extern void ImGui_ImplSDL3_ProcessEvent(const SDL_Event *);
    ImGui_ImplSDL3_ProcessEvent(&event);

    if (event.type == SDL_EVENT_QUIT) {
      m_shouldClose = true;
    }
    if (event.type == SDL_EVENT_KEY_DOWN &&
        event.key.scancode == SDL_SCANCODE_ESCAPE) {
      m_shouldClose = true;
    }
  }
  return !m_shouldClose;
}

void VulkanContext::initWindow(int width, int height, const std::string &title) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    throw std::runtime_error(std::string("Failed to initialize SDL3: ") + SDL_GetError());
  }

  m_window = SDL_CreateWindow(title.c_str(), width, height,
                              SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  if (!m_window) {
    throw std::runtime_error(std::string("Failed to create SDL3 window: ") + SDL_GetError());
  }
}

void VulkanContext::initVulkan() {
  createInstance();
  createSurface();
  pickPhysicalDevice();
  createLogicalDevice();
  createSwapChain();
  createImageViews();
  createCommandPool();
}

void VulkanContext::createInstance() {
  vk::ApplicationInfo appInfo(
      "CRIX Visualizer",
      VK_MAKE_VERSION(1, 0, 0),
      "CRIX Engine",
      VK_MAKE_VERSION(1, 0, 0),
      VK_API_VERSION_1_2
  );

  uint32_t extCount = 0;
  const char *const *sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);
  std::vector<const char *> extensions(sdlExts, sdlExts + extCount);

  vk::InstanceCreateInfo createInfo({}, &appInfo, 0, nullptr, static_cast<uint32_t>(extensions.size()), extensions.data());
  m_instance = vk::createInstance(createInfo);
}

void VulkanContext::createSurface() {
  VkSurfaceKHR rawSurface;
  if (!SDL_Vulkan_CreateSurface(m_window, static_cast<VkInstance>(m_instance),
                                nullptr, &rawSurface)) {
    throw std::runtime_error(std::string("Failed to create Vulkan surface via SDL3: ") +
                             SDL_GetError());
  }
  m_surface = rawSurface;
}

QueueFamilyIndices VulkanContext::findQueueFamilies(vk::PhysicalDevice device) {
  QueueFamilyIndices indices;
  auto queueFamilies = device.getQueueFamilyProperties();

  uint32_t i = 0;
  for (const auto &qf : queueFamilies) {
    if (qf.queueFlags & vk::QueueFlagBits::eGraphics) {
      indices.graphicsFamily = i;
    }
    if (device.getSurfaceSupportKHR(i, m_surface)) {
      indices.presentFamily = i;
    }
    if (indices.isComplete()) break;
    i++;
  }
  return indices;
}

SwapChainSupportDetails VulkanContext::querySwapChainSupport(vk::PhysicalDevice device) {
  SwapChainSupportDetails details;
  details.capabilities = device.getSurfaceCapabilitiesKHR(m_surface);
  details.formats = device.getSurfaceFormatsKHR(m_surface);
  details.presentModes = device.getSurfacePresentModesKHR(m_surface);
  return details;
}

void VulkanContext::pickPhysicalDevice() {
  auto devices = m_instance.enumeratePhysicalDevices();
  if (devices.empty()) {
    throw std::runtime_error("Failed to find GPUs with Vulkan support.");
  }

  for (const auto &device : devices) {
    auto indices = findQueueFamilies(device);
    if (indices.isComplete()) {
      auto swapChainSupport = querySwapChainSupport(device);
      if (!swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty()) {
        m_physicalDevice = device;
        break;
      }
    }
  }

  if (!m_physicalDevice) {
    throw std::runtime_error("Failed to find a suitable GPU.");
  }
}

void VulkanContext::createLogicalDevice() {
  auto indices = findQueueFamilies(m_physicalDevice);

  std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    queueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags(), queueFamily, 1, &queuePriority);
  }

  vk::PhysicalDeviceFeatures deviceFeatures{};
  vk::DeviceCreateInfo createInfo(
      {},
      static_cast<uint32_t>(queueCreateInfos.size()), queueCreateInfos.data(),
      0, nullptr,
      static_cast<uint32_t>(deviceExtensions.size()), deviceExtensions.data(),
      &deviceFeatures
  );

  m_device = m_physicalDevice.createDevice(createInfo);
  m_graphicsQueueFamily = indices.graphicsFamily.value();
  m_graphicsQueue = m_device.getQueue(indices.graphicsFamily.value(), 0);
  m_presentQueue = m_device.getQueue(indices.presentFamily.value(), 0);
}

vk::SurfaceFormatKHR VulkanContext::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
  for (const auto &availableFormat : availableFormats) {
    if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
        availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
      return availableFormat;
    }
  }
  return availableFormats[0];
}

vk::PresentModeKHR VulkanContext::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes) {
  for (const auto &mode : availablePresentModes) {
    if (mode == vk::PresentModeKHR::eMailbox) {
      return mode;
    }
  }
  return vk::PresentModeKHR::eFifo;
}

vk::Extent2D VulkanContext::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities) {
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  }
  int width, height;
  SDL_GetWindowSizeInPixels(m_window, &width, &height);

  vk::Extent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
  actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
  actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
  return actualExtent;
}

void VulkanContext::createSwapChain() {
  SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_physicalDevice);

  vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
  vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
  vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

  uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
  if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
    imageCount = swapChainSupport.capabilities.maxImageCount;
  }

  QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

  vk::SwapchainCreateInfoKHR createInfo(
      {},
      m_surface,
      imageCount,
      surfaceFormat.format,
      surfaceFormat.colorSpace,
      extent,
      1,
      vk::ImageUsageFlagBits::eColorAttachment
  );

  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = vk::SharingMode::eExclusive;
  }

  createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
  createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;

  m_swapChain = m_device.createSwapchainKHR(createInfo);
  m_swapChainImages = m_device.getSwapchainImagesKHR(m_swapChain);
  m_swapChainImageFormat = surfaceFormat.format;
  m_swapChainExtent = extent;
}

void VulkanContext::createImageViews() {
  m_swapChainImageViews.resize(m_swapChainImages.size());

  for (size_t i = 0; i < m_swapChainImages.size(); i++) {
    vk::ImageViewCreateInfo createInfo(
        {},
        m_swapChainImages[i],
        vk::ImageViewType::e2D,
        m_swapChainImageFormat,
        vk::ComponentMapping{},
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
    );
    m_swapChainImageViews[i] = m_device.createImageView(createInfo);
  }
}

void VulkanContext::createCommandPool() {
  QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_physicalDevice);

  vk::CommandPoolCreateInfo poolInfo(
      vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      queueFamilyIndices.graphicsFamily.value()
  );
  m_commandPool = m_device.createCommandPool(poolInfo);
}

void VulkanContext::cleanupSwapChain() {
  for (auto imageView : m_swapChainImageViews) {
    m_device.destroyImageView(imageView);
  }
  m_swapChainImageViews.clear();

  if (m_swapChain) {
    m_device.destroySwapchainKHR(m_swapChain);
    m_swapChain = nullptr;
  }
}

void VulkanContext::recreateSwapChain() {
  int width = 0, height = 0;
  SDL_GetWindowSizeInPixels(m_window, &width, &height);
  while (width == 0 || height == 0) {
    SDL_WaitEvent(nullptr);
    SDL_GetWindowSizeInPixels(m_window, &width, &height);
  }

  m_device.waitIdle();
  cleanupSwapChain();
  createSwapChain();
  createImageViews();
}
