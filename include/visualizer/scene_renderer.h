#ifndef SCENE_RENDERER_H
#define SCENE_RENDERER_H

#include "visualizer/vk_context.h"
#include "visualizer/vk_pipeline.h"
#include "trace_model.h"

#include <vector>
#include <array>

struct ElementInstanceData {
  float posX, posY;
  float sizeX, sizeY;
  float color[4];
  float isMutated;
  float pad[3];
};

struct GlobalUniforms {
  float projection[16];
  float time;
  float highlightIntensity;
  float pad[2];
};

class SceneRenderer {
public:
  SceneRenderer(VulkanContext &context, VulkanPipeline &pipeline);
  ~SceneRenderer();

  void updateDataFromStep(const StepRecord &step, float animationProgress,
                          const std::string &targetDataStructure = "");
  void render(uint32_t imageIndex, vk::CommandBuffer cmd);

private:
  void createBuffers();
  void createDescriptorPoolAndSets();
  uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

  VulkanContext &m_context;
  VulkanPipeline &m_pipeline;

  vk::Buffer m_quadVertexBuffer;
  vk::DeviceMemory m_quadVertexMemory;

  vk::Buffer m_uniformBuffer;
  vk::DeviceMemory m_uniformMemory;

  vk::Buffer m_instanceBuffer;
  vk::DeviceMemory m_instanceMemory;

  vk::DescriptorPool m_descriptorPool;
  vk::DescriptorSet m_descriptorSet;

  std::vector<ElementInstanceData> m_instances;
};

#endif // SCENE_RENDERER_H
