#include "visualizer/vk_pipeline.h"
#include <fstream>
#include <stdexcept>
#include <iostream>

static std::vector<char> readFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file: " + filename);
  }
  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);
  file.seekg(0);
  file.read(buffer.data(), fileSize);
  return buffer;
}

VulkanPipeline::VulkanPipeline(vk::Device device, vk::Extent2D extent, vk::Format format,
                               const std::string &vertSpvPath, const std::string &fragSpvPath)
    : m_device(device) {
  createRenderPass(format);
  createDescriptorSetLayout();
  createGraphicsPipeline(extent, vertSpvPath, fragSpvPath);
}

VulkanPipeline::~VulkanPipeline() {
  if (m_pipeline) m_device.destroyPipeline(m_pipeline);
  if (m_layout) m_device.destroyPipelineLayout(m_layout);
  if (m_descriptorSetLayout) m_device.destroyDescriptorSetLayout(m_descriptorSetLayout);
  if (m_renderPass) m_device.destroyRenderPass(m_renderPass);
}

void VulkanPipeline::createRenderPass(vk::Format format) {
  vk::AttachmentDescription colorAttachment(
      {},
      format,
      vk::SampleCountFlagBits::e1,
      vk::AttachmentLoadOp::eClear,
      vk::AttachmentStoreOp::eStore,
      vk::AttachmentLoadOp::eDontCare,
      vk::AttachmentStoreOp::eDontCare,
      vk::ImageLayout::eUndefined,
      vk::ImageLayout::ePresentSrcKHR
  );

  vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);

  vk::SubpassDescription subpass(
      {},
      vk::PipelineBindPoint::eGraphics,
      0, nullptr,
      1, &colorAttachmentRef
  );

  vk::SubpassDependency dependency(
      VK_SUBPASS_EXTERNAL,
      0,
      vk::PipelineStageFlagBits::eColorAttachmentOutput,
      vk::PipelineStageFlagBits::eColorAttachmentOutput,
      {},
      vk::AccessFlagBits::eColorAttachmentWrite
  );

  vk::RenderPassCreateInfo renderPassInfo({}, 1, &colorAttachment, 1, &subpass, 1, &dependency);
  m_renderPass = m_device.createRenderPass(renderPassInfo);
}

void VulkanPipeline::createDescriptorSetLayout() {
  // Binding 0: Uniform buffer (Projection, Time, etc.)
  vk::DescriptorSetLayoutBinding uboLayoutBinding(
      0,
      vk::DescriptorType::eUniformBuffer,
      1,
      vk::ShaderStageFlagBits::eVertex
  );

  // Binding 1: StructuredBuffer for Element instances
  vk::DescriptorSetLayoutBinding ssboLayoutBinding(
      1,
      vk::DescriptorType::eStorageBuffer,
      1,
      vk::ShaderStageFlagBits::eVertex
  );

  std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, ssboLayoutBinding};
  vk::DescriptorSetLayoutCreateInfo layoutInfo({}, static_cast<uint32_t>(bindings.size()), bindings.data());
  m_descriptorSetLayout = m_device.createDescriptorSetLayout(layoutInfo);
}

vk::ShaderModule VulkanPipeline::createShaderModule(const std::vector<char> &code) {
  vk::ShaderModuleCreateInfo createInfo(
      {},
      code.size(),
      reinterpret_cast<const uint32_t *>(code.data())
  );
  return m_device.createShaderModule(createInfo);
}

void VulkanPipeline::createGraphicsPipeline(vk::Extent2D extent, const std::string &vertSpvPath, const std::string &fragSpvPath) {
  auto vertCode = readFile(vertSpvPath);
  auto fragCode = readFile(fragSpvPath);

  vk::ShaderModule vertModule = createShaderModule(vertCode);
  vk::ShaderModule fragModule = createShaderModule(fragCode);

  vk::PipelineShaderStageCreateInfo vertStageInfo({}, vk::ShaderStageFlagBits::eVertex, vertModule, "main");
  vk::PipelineShaderStageCreateInfo fragStageInfo({}, vk::ShaderStageFlagBits::eFragment, fragModule, "main");
  vk::PipelineShaderStageCreateInfo shaderStages[] = {vertStageInfo, fragStageInfo};

  // Vertex input layout: quad vertex coordinates [0..1]
  vk::VertexInputBindingDescription bindingDesc(0, sizeof(float) * 2, vk::VertexInputRate::eVertex);
  vk::VertexInputAttributeDescription attrDesc(0, 0, vk::Format::eR32G32Sfloat, 0);

  vk::PipelineVertexInputStateCreateInfo vertexInputInfo({}, 1, &bindingDesc, 1, &attrDesc);

  vk::PipelineInputAssemblyStateCreateInfo inputAssembly(
      {},
      vk::PrimitiveTopology::eTriangleStrip,
      VK_FALSE
  );

  vk::Viewport viewport(0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f);
  vk::Rect2D scissor({0, 0}, extent);
  vk::PipelineViewportStateCreateInfo viewportState({}, 1, &viewport, 1, &scissor);

  vk::PipelineRasterizationStateCreateInfo rasterizer(
      {},
      VK_FALSE,
      VK_FALSE,
      vk::PolygonMode::eFill,
      vk::CullModeFlagBits::eNone,
      vk::FrontFace::eCounterClockwise,
      VK_FALSE,
      0.0f, 0.0f, 0.0f,
      1.0f
  );

  vk::PipelineMultisampleStateCreateInfo multisampling({}, vk::SampleCountFlagBits::e1, VK_FALSE);

  vk::PipelineColorBlendAttachmentState colorBlendAttachment;
  colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  colorBlendAttachment.blendEnable = VK_TRUE;
  colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
  colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
  colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
  colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
  colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

  vk::PipelineColorBlendStateCreateInfo colorBlending(
      {},
      VK_FALSE,
      vk::LogicOp::eCopy,
      1, &colorBlendAttachment
  );

  vk::PipelineLayoutCreateInfo pipelineLayoutInfo({}, 1, &m_descriptorSetLayout);
  m_layout = m_device.createPipelineLayout(pipelineLayoutInfo);

  vk::GraphicsPipelineCreateInfo pipelineInfo(
      {},
      2, shaderStages,
      &vertexInputInfo,
      &inputAssembly,
      nullptr,
      &viewportState,
      &rasterizer,
      &multisampling,
      nullptr,
      &colorBlending,
      nullptr,
      m_layout,
      m_renderPass,
      0
  );

  auto result = m_device.createGraphicsPipeline(nullptr, pipelineInfo);
  if (result.result != vk::Result::eSuccess) {
    throw std::runtime_error("Failed to create graphics pipeline.");
  }
  m_pipeline = result.value;

  m_device.destroyShaderModule(fragModule);
  m_device.destroyShaderModule(vertModule);
}
