#ifndef VK_PIPELINE_H
#define VK_PIPELINE_H

#include <vulkan/vulkan.hpp>
#include <string>
#include <vector>

class VulkanPipeline {
public:
  VulkanPipeline(vk::Device device, vk::Extent2D extent, vk::Format format,
                 const std::string &vertSpvPath, const std::string &fragSpvPath);
  ~VulkanPipeline();

  vk::Pipeline getPipeline() const { return m_pipeline; }
  vk::PipelineLayout getLayout() const { return m_layout; }
  vk::RenderPass getRenderPass() const { return m_renderPass; }
  vk::DescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout; }

private:
  vk::ShaderModule createShaderModule(const std::vector<char> &code);
  void createRenderPass(vk::Format format);
  void createDescriptorSetLayout();
  void createGraphicsPipeline(vk::Extent2D extent, const std::string &vertSpvPath, const std::string &fragSpvPath);

  vk::Device m_device;
  vk::RenderPass m_renderPass;
  vk::DescriptorSetLayout m_descriptorSetLayout;
  vk::PipelineLayout m_layout;
  vk::Pipeline m_pipeline;
};

#endif // VK_PIPELINE_H
