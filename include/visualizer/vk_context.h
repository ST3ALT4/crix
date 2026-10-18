#ifndef VK_CONTEXT_H
#define VK_CONTEXT_H

#include <vulkan/vulkan.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>
#include <vector>
#include <memory>
#include <optional>

struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  bool isComplete() const {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};

struct SwapChainSupportDetails {
  vk::SurfaceCapabilitiesKHR capabilities;
  std::vector<vk::SurfaceFormatKHR> formats;
  std::vector<vk::PresentModeKHR> presentModes;
};

class VulkanContext {
public:
  VulkanContext(int width, int height, const std::string &title);
  ~VulkanContext();

  // Non-copyable
  VulkanContext(const VulkanContext &) = delete;
  VulkanContext &operator=(const VulkanContext &) = delete;

  bool shouldClose() const;
  void pollEvents();

  GLFWwindow *getWindow() const { return m_window; }
  vk::Instance getInstance() const { return m_instance; }
  vk::Device getDevice() const { return m_device; }
  vk::PhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
  uint32_t getGraphicsQueueFamily() const { return m_graphicsQueueFamily; }
  vk::Queue getGraphicsQueue() const { return m_graphicsQueue; }
  vk::Queue getPresentQueue() const { return m_presentQueue; }
  vk::SwapchainKHR getSwapChain() const { return m_swapChain; }
  vk::Format getSwapChainImageFormat() const { return m_swapChainImageFormat; }
  vk::Extent2D getSwapChainExtent() const { return m_swapChainExtent; }
  const std::vector<vk::ImageView> &getSwapChainImageViews() const { return m_swapChainImageViews; }
  vk::CommandPool getCommandPool() const { return m_commandPool; }

  void recreateSwapChain();

private:
  void initWindow(int width, int height, const std::string &title);
  void initVulkan();
  void createInstance();
  void createSurface();
  void pickPhysicalDevice();
  void createLogicalDevice();
  void createSwapChain();
  void createImageViews();
  void createCommandPool();
  void cleanupSwapChain();

  QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device);
  SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device);
  vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats);
  vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes);
  vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities);

  GLFWwindow *m_window = nullptr;

  vk::Instance m_instance;
  vk::SurfaceKHR m_surface;
  vk::PhysicalDevice m_physicalDevice;
  vk::Device m_device;
  uint32_t m_graphicsQueueFamily = 0;
  vk::Queue m_graphicsQueue;
  vk::Queue m_presentQueue;

  vk::SwapchainKHR m_swapChain;
  std::vector<vk::Image> m_swapChainImages;
  vk::Format m_swapChainImageFormat;
  vk::Extent2D m_swapChainExtent;
  std::vector<vk::ImageView> m_swapChainImageViews;

  vk::CommandPool m_commandPool;
};

#endif // VK_CONTEXT_H
