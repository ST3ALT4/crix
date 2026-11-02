#include "visualizer/scene_renderer.h"
#include <cstring>
#include <iostream>
#include <cmath>

static const float quadVertices[] = {
    0.0f, 0.0f,
    1.0f, 0.0f,
    0.0f, 1.0f,
    1.0f, 1.0f
};

SceneRenderer::SceneRenderer(VulkanContext &context, VulkanPipeline &pipeline)
    : m_context(context), m_pipeline(pipeline) {
  createBuffers();
  createDescriptorPoolAndSets();
}

SceneRenderer::~SceneRenderer() {
  auto device = m_context.getDevice();
  if (m_descriptorPool) device.destroyDescriptorPool(m_descriptorPool);

  if (m_instanceBuffer) device.destroyBuffer(m_instanceBuffer);
  if (m_instanceMemory) device.freeMemory(m_instanceMemory);

  if (m_uniformBuffer) device.destroyBuffer(m_uniformBuffer);
  if (m_uniformMemory) device.freeMemory(m_uniformMemory);

  if (m_quadVertexBuffer) device.destroyBuffer(m_quadVertexBuffer);
  if (m_quadVertexMemory) device.freeMemory(m_quadVertexMemory);
}

uint32_t SceneRenderer::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
  auto memProperties = m_context.getPhysicalDevice().getMemoryProperties();
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }
  throw std::runtime_error("Failed to find suitable memory type.");
}

void SceneRenderer::createBuffers() {
  auto device = m_context.getDevice();

  // 1. Quad Vertex Buffer (Triangle Strip)
  vk::DeviceSize vertexSize = sizeof(quadVertices);
  vk::BufferCreateInfo vBufferInfo({}, vertexSize, vk::BufferUsageFlagBits::eVertexBuffer);
  m_quadVertexBuffer = device.createBuffer(vBufferInfo);

  auto vMemReq = device.getBufferMemoryRequirements(m_quadVertexBuffer);
  vk::MemoryAllocateInfo vAllocInfo(
      vMemReq.size,
      findMemoryType(vMemReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
  );
  m_quadVertexMemory = device.allocateMemory(vAllocInfo);
  device.bindBufferMemory(m_quadVertexBuffer, m_quadVertexMemory, 0);

  void *vData = device.mapMemory(m_quadVertexMemory, 0, vertexSize);
  std::memcpy(vData, quadVertices, vertexSize);
  device.unmapMemory(m_quadVertexMemory);

  // 2. Uniform Buffer
  vk::DeviceSize uboSize = sizeof(GlobalUniforms);
  vk::BufferCreateInfo uBufferInfo({}, uboSize, vk::BufferUsageFlagBits::eUniformBuffer);
  m_uniformBuffer = device.createBuffer(uBufferInfo);

  auto uMemReq = device.getBufferMemoryRequirements(m_uniformBuffer);
  vk::MemoryAllocateInfo uAllocInfo(
      uMemReq.size,
      findMemoryType(uMemReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
  );
  m_uniformMemory = device.allocateMemory(uAllocInfo);
  device.bindBufferMemory(m_uniformBuffer, m_uniformMemory, 0);

  // Set default orthographic projection (Normalized Device Coordinates: [-1, 1])
  GlobalUniforms ubo{};
  std::memset(ubo.projection, 0, sizeof(ubo.projection));
  ubo.projection[0] = 1.0f;
  ubo.projection[5] = 1.0f;
  ubo.projection[10] = 1.0f;
  ubo.projection[15] = 1.0f;
  ubo.time = 0.0f;
  ubo.highlightIntensity = 1.0f;

  void *uData = device.mapMemory(m_uniformMemory, 0, uboSize);
  std::memcpy(uData, &ubo, uboSize);
  device.unmapMemory(m_uniformMemory);

  // 3. Instance SSBO (Space for up to 1024 elements)
  vk::DeviceSize ssboSize = sizeof(ElementInstanceData) * 1024;
  vk::BufferCreateInfo ssboInfo({}, ssboSize, vk::BufferUsageFlagBits::eStorageBuffer);
  m_instanceBuffer = device.createBuffer(ssboInfo);

  auto ssboReq = device.getBufferMemoryRequirements(m_instanceBuffer);
  vk::MemoryAllocateInfo ssboAllocInfo(
      ssboReq.size,
      findMemoryType(ssboReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
  );
  m_instanceMemory = device.allocateMemory(ssboAllocInfo);
  device.bindBufferMemory(m_instanceBuffer, m_instanceMemory, 0);
}

void SceneRenderer::createDescriptorPoolAndSets() {
  auto device = m_context.getDevice();

  std::array<vk::DescriptorPoolSize, 2> poolSizes = {
      vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1),
      vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 1)
  };

  vk::DescriptorPoolCreateInfo poolInfo(
      vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      1,
      static_cast<uint32_t>(poolSizes.size()), poolSizes.data()
  );
  m_descriptorPool = device.createDescriptorPool(poolInfo);

  auto layout = m_pipeline.getDescriptorSetLayout();
  vk::DescriptorSetAllocateInfo allocInfo(m_descriptorPool, 1, &layout);
  m_descriptorSet = device.allocateDescriptorSets(allocInfo)[0];

  // Configure descriptor set bindings
  vk::DescriptorBufferInfo uboBufferInfo(m_uniformBuffer, 0, sizeof(GlobalUniforms));
  vk::WriteDescriptorSet uboWrite(
      m_descriptorSet, 0, 0, 1,
      vk::DescriptorType::eUniformBuffer,
      nullptr, &uboBufferInfo, nullptr
  );

  vk::DescriptorBufferInfo ssboBufferInfo(m_instanceBuffer, 0, sizeof(ElementInstanceData) * 1024);
  vk::WriteDescriptorSet ssboWrite(
      m_descriptorSet, 1, 0, 1,
      vk::DescriptorType::eStorageBuffer,
      nullptr, &ssboBufferInfo, nullptr
  );

  std::array<vk::WriteDescriptorSet, 2> descriptorWrites = {uboWrite, ssboWrite};
  device.updateDescriptorSets(descriptorWrites, nullptr);
}

void SceneRenderer::updateDataFromStep(const StepRecord &step, float animationProgress,
                                        const std::string &targetDataStructure) {
  (void)animationProgress;
  m_instances.clear();

  // Find target array or fall back to first array in current step
  const std::pair<std::string, RuntimeValue> *chosenArray = nullptr;
  if (!targetDataStructure.empty()) {
    for (const auto &var : step.variables) {
      if (var.first == targetDataStructure && var.second.kind == ValueKind::ARRAY &&
          !var.second.elements.empty()) {
        chosenArray = &var;
        break;
      }
    }
  }
  if (!chosenArray) {
    for (const auto &var : step.variables) {
      if (var.second.kind == ValueKind::ARRAY && !var.second.elements.empty()) {
        chosenArray = &var;
        break;
      }
    }
  }

  if (chosenArray) {
    const auto &var = *chosenArray;
    size_t count = var.second.elements.size();
      float startX = -0.8f;
      float totalWidth = 1.6f;
      float spacing = totalWidth / static_cast<float>(count);
      float barWidth = spacing * 0.75f;

      for (size_t i = 0; i < count; ++i) {
        float val = 1.0f;
        try {
          val = std::stof(var.second.elements[i].raw_value);
        } catch (...) {
          val = 1.0f;
        }

        // Normalize bar height
        float height = std::max(0.1f, val * 0.15f);
        float posX = startX + static_cast<float>(i) * spacing;
        float posY = -0.6f;

        ElementInstanceData inst{};
        inst.posX = posX;
        inst.posY = posY;
        inst.sizeX = barWidth;
        inst.sizeY = height;

        // Colors: Blue base, highlight if mutated
        inst.color[0] = 0.2f;
        inst.color[1] = 0.6f;
        inst.color[2] = 0.9f;
        inst.color[3] = 1.0f;
        inst.isMutated = 0.0f;

        for (const auto &mut : step.delta.array_mutations) {
          if (mut.variable_name == var.first && mut.index == i) {
            inst.isMutated = 1.0f;
            float pulse = 0.5f + 0.5f * std::sin(animationProgress * 3.14159265f);
            inst.color[0] = 0.95f + 0.05f * pulse;
            inst.color[1] = 0.25f * (1.0f - animationProgress);
            inst.color[2] = 0.35f * (1.0f - animationProgress);
            break;
          }
        }

        m_instances.push_back(inst);
      }
  }

  // Upload instances to SSBO
  if (!m_instances.empty()) {
    auto device = m_context.getDevice();
    size_t uploadSize = sizeof(ElementInstanceData) * m_instances.size();
    void *data = device.mapMemory(m_instanceMemory, 0, uploadSize);
    std::memcpy(data, m_instances.data(), uploadSize);
    device.unmapMemory(m_instanceMemory);
  }
}

void SceneRenderer::render(uint32_t imageIndex, vk::CommandBuffer cmd) {
  (void)imageIndex;
  if (m_instances.empty()) return;

  cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.getPipeline());
  cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline.getLayout(), 0, 1, &m_descriptorSet, 0, nullptr);

  vk::Buffer vertexBuffers[] = {m_quadVertexBuffer};
  vk::DeviceSize offsets[] = {0};
  cmd.bindVertexBuffers(0, 1, vertexBuffers, offsets);

  cmd.draw(4, static_cast<uint32_t>(m_instances.size()), 0, 0);
}
