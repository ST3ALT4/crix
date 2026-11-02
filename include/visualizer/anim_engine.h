#ifndef ANIM_ENGINE_H
#define ANIM_ENGINE_H

#include "visualizer/visual_model.h"
#include <imgui.h>

// Modular, project-agnostic animation & visualization renderer.
// Draws generic VisualScenes using 100% solid colors and adaptive layouts.
class AnimationEngine {
public:
  AnimationEngine() = default;
  ~AnimationEngine() = default;

  // Render a visual scene into the current ImGui window / canvas area
  void renderScene(ImDrawList *drawList, const VisualScene &scene,
                   ImVec2 canvasOrigin, ImVec2 canvasSize,
                   float animationProgress);

private:
  void renderArrayLayout(ImDrawList *drawList, const VisualScene &scene,
                         ImVec2 origin, ImVec2 size, float progress);
  void renderLinkedListLayout(ImDrawList *drawList, const VisualScene &scene,
                              ImVec2 origin, ImVec2 size, float progress);
  void renderBinaryTreeLayout(ImDrawList *drawList, const VisualScene &scene,
                              ImVec2 origin, ImVec2 size, float progress);
  void renderGrid2DLayout(ImDrawList *drawList, const VisualScene &scene,
                          ImVec2 origin, ImVec2 size, float progress);
  void renderRecursionTreeLayout(ImDrawList *drawList, const VisualScene &scene,
                                 ImVec2 origin, ImVec2 size, float progress);
  void renderStructLayout(ImDrawList *drawList, const VisualScene &scene,
                          ImVec2 origin, ImVec2 size, float progress);
  void renderPointerLayout(ImDrawList *drawList, const VisualScene &scene,
                           ImVec2 origin, ImVec2 size, float progress);
  void renderScalarLayout(ImDrawList *drawList, const VisualScene &scene,
                          ImVec2 origin, ImVec2 size, float progress);
  void renderEmptyState(ImDrawList *drawList, ImVec2 origin, ImVec2 size);
};

#endif // ANIM_ENGINE_H
