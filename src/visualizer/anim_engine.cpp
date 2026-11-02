#include "visualizer/anim_engine.h"
#include <algorithm>
#include <cmath>
#include <string>

// Solid Color Constants (100% Opacity / alpha = 255)
namespace SolidCol {
static constexpr ImU32 CanvasBg       = IM_COL32(15, 23, 42, 255);    // Slate 900
static constexpr ImU32 CardBg         = IM_COL32(30, 41, 59, 255);    // Slate 800
static constexpr ImU32 CardBorder     = IM_COL32(71, 85, 105, 255);   // Slate 600
static constexpr ImU32 MutatedBg      = IM_COL32(190, 18, 60, 255);   // Rose 700
static constexpr ImU32 MutatedBorder  = IM_COL32(244, 63, 94, 255);   // Rose 500
static constexpr ImU32 Primary        = IM_COL32(37, 99, 235, 255);   // Blue 600
static constexpr ImU32 PrimaryLight   = IM_COL32(96, 165, 250, 255);  // Blue 400
static constexpr ImU32 TextWhite      = IM_COL32(248, 250, 252, 255); // Slate 50
static constexpr ImU32 TextMuted      = IM_COL32(148, 163, 184, 255); // Slate 400
static constexpr ImU32 TextDim        = IM_COL32(100, 116, 139, 255); // Slate 500

static const ImU32 PointerColors[] = {
    IM_COL32(217, 119, 6, 255),   // Solid Amber
    IM_COL32(16, 185, 129, 255),  // Solid Emerald
    IM_COL32(14, 165, 233, 255),  // Solid Sky
    IM_COL32(168, 85, 247, 255),  // Solid Purple
    IM_COL32(236, 72, 153, 255),  // Solid Pink
};
static constexpr size_t NumPointerColors = 5;
} // namespace SolidCol

void AnimationEngine::renderScene(ImDrawList *drawList, const VisualScene &scene,
                                  ImVec2 canvasOrigin, ImVec2 canvasSize,
                                  float animationProgress) {
  if (scene.is_empty || scene.nodes.empty()) {
    renderEmptyState(drawList, canvasOrigin, canvasSize);
    return;
  }

  // Draw solid canvas background
  drawList->AddRectFilled(canvasOrigin,
                          ImVec2(canvasOrigin.x + canvasSize.x,
                                 canvasOrigin.y + canvasSize.y),
                          SolidCol::CanvasBg, 8.0f);
  drawList->AddRect(canvasOrigin,
                    ImVec2(canvasOrigin.x + canvasSize.x,
                           canvasOrigin.y + canvasSize.y),
                    SolidCol::CardBorder, 8.0f, 0, 1.5f);

  // Scene Title & Type Header
  if (!scene.title.empty()) {
    std::string header = scene.title;
    if (!scene.type_name.empty()) {
      header += " (" + scene.type_name + ")";
    }
    if (!scene.graph.description.empty()) {
      header += "  |  DSA: " + scene.graph.description;
    }
    drawList->AddText(ImVec2(canvasOrigin.x + 16, canvasOrigin.y + 12),
                      SolidCol::TextWhite, header.c_str());
  }

  // Check top-level detected DSA topology first
  if (scene.graph.topology == DSATopology::LINKED_LIST) {
    renderLinkedListLayout(drawList, scene, canvasOrigin, canvasSize, animationProgress);
    return;
  }
  if (scene.graph.topology == DSATopology::BINARY_TREE) {
    renderBinaryTreeLayout(drawList, scene, canvasOrigin, canvasSize, animationProgress);
    return;
  }
  if (scene.graph.topology == DSATopology::GRID_2D) {
    renderGrid2DLayout(drawList, scene, canvasOrigin, canvasSize, animationProgress);
    return;
  }
  if (scene.graph.topology == DSATopology::RECURSION_TREE) {
    renderRecursionTreeLayout(drawList, scene, canvasOrigin, canvasSize, animationProgress);
    return;
  }

  // Determine dominant node kind to pick layout
  VisualNodeKind dominantKind = scene.nodes[0].kind;
  switch (dominantKind) {
  case VisualNodeKind::ARRAY_CELL:
    renderArrayLayout(drawList, scene, canvasOrigin, canvasSize,
                      animationProgress);
    break;
  case VisualNodeKind::STRUCT_RECORD:
    renderStructLayout(drawList, scene, canvasOrigin, canvasSize,
                       animationProgress);
    break;
  case VisualNodeKind::POINTER_LINK:
    renderPointerLayout(drawList, scene, canvasOrigin, canvasSize,
                        animationProgress);
    break;
  case VisualNodeKind::SCALAR:
  default:
    renderScalarLayout(drawList, scene, canvasOrigin, canvasSize,
                       animationProgress);
    break;
  }
}

void AnimationEngine::renderArrayLayout(ImDrawList *drawList,
                                        const VisualScene &scene,
                                        ImVec2 origin, ImVec2 size,
                                        float progress) {
  const size_t count = scene.nodes.size();
  if (count == 0) return;

  const float paddingX = 24.0f;
  const float availW = size.x - paddingX * 2.0f;
  const float slotW = std::min(110.0f, availW / static_cast<float>(count));
  const float cardW = std::max(48.0f, slotW - 12.0f);
  const float cardH = 68.0f;
  const float totalLayoutW = static_cast<float>(count) * slotW;
  const float startX = origin.x + paddingX + (availW - totalLayoutW) * 0.5f;
  const float centerY = origin.y + size.y * 0.52f;

  for (size_t i = 0; i < count; ++i) {
    const auto &node = scene.nodes[i];
    float cx = startX + static_cast<float>(i) * slotW + (slotW - cardW) * 0.5f;
    float cy = centerY - cardH * 0.5f;

    // Apply animation pulse on mutation
    ImU32 fillCol = SolidCol::CardBg;
    ImU32 borderCol = SolidCol::CardBorder;

    // Check if outside active search/sliding window
    if (scene.graph.window_start != -1 && scene.graph.window_end != -1) {
      if (static_cast<int64_t>(i) < scene.graph.window_start ||
          static_cast<int64_t>(i) > scene.graph.window_end) {
        fillCol = IM_COL32(18, 24, 38, 255); // Dimmed out
        borderCol = IM_COL32(40, 50, 70, 255);
      } else {
        fillCol = IM_COL32(26, 38, 64, 255); // Highlighted active window
        borderCol = IM_COL32(59, 130, 246, 255);
      }
    }

    // Spotlight mid / focus index
    if (static_cast<int64_t>(i) == scene.graph.focus_index) {
      borderCol = IM_COL32(245, 158, 11, 255); // Solid Amber border
    }

    if (node.is_mutated) {
      float pulse = 0.5f + 0.5f * std::sin(progress * 3.14159f);
      if (pulse > 0.2f) {
        fillCol = SolidCol::MutatedBg;
        borderCol = SolidCol::MutatedBorder;
      }
    }

    // Main Card Box (Solid fill + Solid border)
    ImVec2 minPt(cx, cy);
    ImVec2 maxPt(cx + cardW, cy + cardH);
    drawList->AddRectFilled(minPt, maxPt, fillCol, 6.0f);
    drawList->AddRect(minPt, maxPt, borderCol, 6.0f, 0, node.is_mutated ? 2.5f : 1.5f);

    // Value text (Centered)
    ImVec2 valTextSize = ImGui::CalcTextSize(node.value.c_str());
    ImVec2 valPos(cx + (cardW - valTextSize.x) * 0.5f,
                  cy + (cardH - valTextSize.y) * 0.40f);
    drawList->AddText(valPos, SolidCol::TextWhite, node.value.c_str());

    // Index Badge below card: "[i]"
    std::string idxStr = "[" + std::to_string(i) + "]";
    ImVec2 idxSize = ImGui::CalcTextSize(idxStr.c_str());
    ImVec2 idxPos(cx + (cardW - idxSize.x) * 0.5f, cy + cardH + 8.0f);
    drawList->AddText(idxPos, SolidCol::TextMuted, idxStr.c_str());
  }

  // Draw Visual Pointers (e.g. iterators 'i', 'j') above cards
  for (size_t pIdx = 0; pIdx < scene.pointers.size(); ++pIdx) {
    const auto &ptr = scene.pointers[pIdx];
    if (ptr.target_index < 0 || ptr.target_index >= static_cast<int64_t>(count))
      continue;

    float cx = startX + static_cast<float>(ptr.target_index) * slotW + (slotW - cardW) * 0.5f;
    float cardTopY = centerY - cardH * 0.5f;
    float pointerY = cardTopY - 32.0f - static_cast<float>(pIdx % 2) * 26.0f;
    float arrowTipX = cx + cardW * 0.5f;

    ImU32 ptrColor = (ptr.color_rgba != 0)
                         ? ptr.color_rgba
                         : SolidCol::PointerColors[pIdx % SolidCol::NumPointerColors];

    // Pointer badge box
    std::string badgeText = ptr.label + " = " + std::to_string(ptr.target_index);
    ImVec2 badgeTextSize = ImGui::CalcTextSize(badgeText.c_str());
    float badgePadX = 8.0f;
    float badgePadY = 3.0f;
    float badgeW = badgeTextSize.x + badgePadX * 2.0f;
    float badgeH = badgeTextSize.y + badgePadY * 2.0f;
    float badgeLeft = arrowTipX - badgeW * 0.5f;

    ImVec2 bMin(badgeLeft, pointerY - badgeH);
    ImVec2 bMax(badgeLeft + badgeW, pointerY);
    drawList->AddRectFilled(bMin, bMax, ptrColor, 4.0f);
    drawList->AddText(ImVec2(badgeLeft + badgePadX, pointerY - badgeH + badgePadY),
                      SolidCol::TextWhite, badgeText.c_str());

    // Solid downward pointer arrow
    drawList->AddTriangleFilled(
        ImVec2(arrowTipX - 5.0f, pointerY),
        ImVec2(arrowTipX + 5.0f, pointerY),
        ImVec2(arrowTipX, cardTopY - 4.0f),
        ptrColor);
  }

  // Draw active comparison bracket if two elements are being compared
  if (scene.graph.compare_a >= 0 && scene.graph.compare_b >= 0 &&
      static_cast<size_t>(scene.graph.compare_a) < count &&
      static_cast<size_t>(scene.graph.compare_b) < count &&
      scene.graph.compare_a != scene.graph.compare_b) {
    float ax = startX + static_cast<float>(scene.graph.compare_a) * slotW + (slotW - cardW) * 0.5f + cardW * 0.5f;
    float bx = startX + static_cast<float>(scene.graph.compare_b) * slotW + (slotW - cardW) * 0.5f + cardW * 0.5f;
    float bracketY = centerY - cardH * 0.5f - 8.0f;
    float archY = bracketY - 14.0f;

    drawList->AddLine(ImVec2(ax, bracketY), ImVec2(ax, archY), SolidCol::PrimaryLight, 2.0f);
    drawList->AddLine(ImVec2(ax, archY), ImVec2(bx, archY), SolidCol::PrimaryLight, 2.0f);
    drawList->AddLine(ImVec2(bx, archY), ImVec2(bx, bracketY), SolidCol::PrimaryLight, 2.0f);

    const char *cmpText = "compare";
    ImVec2 cSize = ImGui::CalcTextSize(cmpText);
    drawList->AddText(ImVec2((ax + bx - cSize.x) * 0.5f, archY - cSize.y - 2.0f),
                      SolidCol::TextMuted, cmpText);
  }
}

void AnimationEngine::renderStructLayout(ImDrawList *drawList,
                                         const VisualScene &scene,
                                         ImVec2 origin, ImVec2 size,
                                         float progress) {
  (void)progress;
  const size_t count = scene.nodes.size();
  if (count == 0) return;

  const float padding = 20.0f;
  const float cardW = std::min(280.0f, (size.x - padding * 3.0f) / std::min((size_t)3, count));
  const float startY = origin.y + 45.0f;

  for (size_t i = 0; i < count; ++i) {
    const auto &node = scene.nodes[i];
    float cx = origin.x + padding + static_cast<float>(i) * (cardW + 16.0f);
    if (cx + cardW > origin.x + size.x) break; // Keep inside bounds

    float fieldH = 26.0f;
    float totalH = 40.0f + static_cast<float>(node.properties.size()) * fieldH + 12.0f;

    ImVec2 minPt(cx, startY);
    ImVec2 maxPt(cx + cardW, startY + totalH);

    // Composite Struct Card (Solid background + Solid border)
    drawList->AddRectFilled(minPt, maxPt, SolidCol::CardBg, 8.0f);
    drawList->AddRect(minPt, maxPt,
                      node.is_mutated ? SolidCol::MutatedBorder : SolidCol::CardBorder,
                      8.0f, 0, node.is_mutated ? 2.5f : 1.5f);

    // Struct Header (Label + Type)
    std::string title = node.label.empty() ? ("Item [" + std::to_string(i) + "]") : node.label;
    drawList->AddText(ImVec2(cx + 12.0f, startY + 10.0f), SolidCol::PrimaryLight, title.c_str());
    drawList->AddLine(ImVec2(cx + 1, startY + 32.0f), ImVec2(cx + cardW - 1, startY + 32.0f),
                      SolidCol::CardBorder, 1.0f);

    // Struct Properties / Fields
    float fieldY = startY + 38.0f;
    for (const auto &prop : node.properties) {
      ImU32 valCol = prop.is_mutated ? SolidCol::MutatedBorder : SolidCol::TextWhite;

      // Field Name
      drawList->AddText(ImVec2(cx + 14.0f, fieldY), SolidCol::TextMuted, prop.name.c_str());

      // Field Value (Right-aligned)
      ImVec2 vSize = ImGui::CalcTextSize(prop.value.c_str());
      drawList->AddText(ImVec2(cx + cardW - 14.0f - vSize.x, fieldY), valCol, prop.value.c_str());

      fieldY += fieldH;
    }
  }
}

void AnimationEngine::renderPointerLayout(ImDrawList *drawList,
                                          const VisualScene &scene,
                                          ImVec2 origin, ImVec2 size,
                                          float progress) {
  (void)progress;
  if (scene.nodes.empty()) return;

  const float padding = 24.0f;
  const float centerY = origin.y + size.y * 0.50f;
  const float nodeW = 160.0f;
  const float nodeH = 75.0f;

  for (size_t i = 0; i < scene.nodes.size(); ++i) {
    const auto &node = scene.nodes[i];
    float cx = origin.x + padding + static_cast<float>(i) * (nodeW + 80.0f);
    if (cx + nodeW > origin.x + size.x) break;

    float cy = centerY - nodeH * 0.5f;
    ImVec2 minPt(cx, cy);
    ImVec2 maxPt(cx + nodeW, cy + nodeH);

    // Pointer Card
    drawList->AddRectFilled(minPt, maxPt, SolidCol::CardBg, 6.0f);
    drawList->AddRect(minPt, maxPt,
                      node.is_mutated ? SolidCol::MutatedBorder : SolidCol::Primary,
                      6.0f, 0, 2.0f);

    // Name & Pointer Symbol
    std::string pTitle = "*" + node.label;
    drawList->AddText(ImVec2(cx + 12.0f, cy + 10.0f), SolidCol::PrimaryLight, pTitle.c_str());

    // Dereferenced value
    std::string valStr = "Points To: " + node.value;
    drawList->AddText(ImVec2(cx + 12.0f, cy + 34.0f), SolidCol::TextWhite, valStr.c_str());

    // Type
    if (!node.type.empty()) {
      drawList->AddText(ImVec2(cx + 12.0f, cy + 52.0f), SolidCol::TextDim, node.type.c_str());
    }

    // Pointer connector arrow to next node if present
    if (i + 1 < scene.nodes.size()) {
      float arrowStartX = cx + nodeW + 10.0f;
      float arrowEndX = arrowStartX + 60.0f;
      drawList->AddLine(ImVec2(arrowStartX, centerY), ImVec2(arrowEndX, centerY),
                        SolidCol::PrimaryLight, 2.5f);
      drawList->AddTriangleFilled(
          ImVec2(arrowEndX, centerY - 5.0f),
          ImVec2(arrowEndX, centerY + 5.0f),
          ImVec2(arrowEndX + 8.0f, centerY),
          SolidCol::PrimaryLight);
    }
  }
}

void AnimationEngine::renderScalarLayout(ImDrawList *drawList,
                                         const VisualScene &scene,
                                         ImVec2 origin, ImVec2 size,
                                         float progress) {
  (void)progress;
  const size_t count = scene.nodes.size();
  if (count == 0) return;

  const float padding = 20.0f;
  const float cardW = std::min(180.0f, (size.x - padding * 2.0f) / std::min((size_t)4, count));
  const float cardH = 90.0f;
  const float centerY = origin.y + size.y * 0.45f;

  for (size_t i = 0; i < count; ++i) {
    const auto &node = scene.nodes[i];
    float cx = origin.x + padding + static_cast<float>(i) * (cardW + 16.0f);
    if (cx + cardW > origin.x + size.x) break;

    float cy = centerY - cardH * 0.5f;
    ImVec2 minPt(cx, cy);
    ImVec2 maxPt(cx + cardW, cy + cardH);

    ImU32 fillCol = node.is_mutated ? SolidCol::MutatedBg : SolidCol::CardBg;
    ImU32 borderCol = node.is_mutated ? SolidCol::MutatedBorder : SolidCol::CardBorder;

    drawList->AddRectFilled(minPt, maxPt, fillCol, 8.0f);
    drawList->AddRect(minPt, maxPt, borderCol, 8.0f, 0, node.is_mutated ? 2.5f : 1.5f);

    // Label
    drawList->AddText(ImVec2(cx + 12.0f, cy + 10.0f), SolidCol::TextMuted, node.label.c_str());

    // Value
    drawList->AddText(ImVec2(cx + 12.0f, cy + 32.0f), SolidCol::TextWhite, node.value.c_str());

    // Type
    if (!node.type.empty()) {
      drawList->AddText(ImVec2(cx + 12.0f, cy + 62.0f), SolidCol::TextDim, node.type.c_str());
    }
  }
}

void AnimationEngine::renderLinkedListLayout(ImDrawList *drawList,
                                            const VisualScene &scene,
                                            ImVec2 origin, ImVec2 size,
                                            float progress) {
  (void)progress;
  const auto &nodes = scene.graph.nodes;
  if (nodes.empty()) return;

  const float paddingX = 40.0f;
  const float cardW = 110.0f;
  const float cardH = 56.0f;
  const float spacing = 60.0f;
  const float centerY = origin.y + size.y * 0.5f;

  std::unordered_map<std::string, ImVec2> nodeCenters;

  for (size_t i = 0; i < nodes.size(); ++i) {
    const auto &node = nodes[i];
    float cx = origin.x + paddingX + static_cast<float>(i) * (cardW + spacing);
    if (cx + cardW > origin.x + size.x) break;
    float cy = centerY - cardH * 0.5f;

    nodeCenters[node.id] = ImVec2(cx + cardW * 0.5f, cy + cardH * 0.5f);

    // Node Box
    ImVec2 minPt(cx, cy);
    ImVec2 maxPt(cx + cardW, cy + cardH);
    ImU32 fillCol = node.is_mutated ? SolidCol::MutatedBg : SolidCol::CardBg;
    ImU32 borderCol = node.is_mutated ? SolidCol::MutatedBorder : SolidCol::CardBorder;

    drawList->AddRectFilled(minPt, maxPt, fillCol, 6.0f);
    drawList->AddRect(minPt, maxPt, borderCol, 6.0f, 0, 1.5f);

    // Node Value
    ImVec2 valSize = ImGui::CalcTextSize(node.label.c_str());
    drawList->AddText(ImVec2(cx + (cardW - valSize.x) * 0.5f, cy + (cardH - valSize.y) * 0.4f),
                      SolidCol::TextWhite, node.label.c_str());

    // Cursors / Pointer badges above node
    for (size_t cIdx = 0; cIdx < node.pointer_labels.size(); ++cIdx) {
      const std::string &pName = node.pointer_labels[cIdx];
      float bY = cy - 24.0f - static_cast<float>(cIdx) * 20.0f;
      std::string bText = "• " + pName;
      drawList->AddText(ImVec2(cx + 6.0f, bY), SolidCol::PrimaryLight, bText.c_str());
    }
  }

  // Draw Edges with arrowheads
  for (const auto &edge : scene.graph.edges) {
    auto fromIt = nodeCenters.find(edge.from_id);
    auto toIt = nodeCenters.find(edge.to_id);
    if (fromIt != nodeCenters.end() && toIt != nodeCenters.end()) {
      ImVec2 start(fromIt->second.x + cardW * 0.5f, fromIt->second.y);
      ImVec2 end(toIt->second.x - cardW * 0.5f, toIt->second.y);

      drawList->AddLine(start, end, SolidCol::PrimaryLight, 2.0f);
      drawList->AddTriangleFilled(
          end,
          ImVec2(end.x - 8.0f, end.y - 5.0f),
          ImVec2(end.x - 8.0f, end.y + 5.0f),
          SolidCol::PrimaryLight);

      if (!edge.label.empty()) {
        ImVec2 midPt((start.x + end.x) * 0.5f, start.y - 14.0f);
        drawList->AddText(midPt, SolidCol::TextMuted, edge.label.c_str());
      }
    }
  }
}

void AnimationEngine::renderBinaryTreeLayout(ImDrawList *drawList,
                                            const VisualScene &scene,
                                            ImVec2 origin, ImVec2 size,
                                            float progress) {
  (void)progress;
  const auto &nodes = scene.graph.nodes;
  if (nodes.empty()) return;

  const float rootX = origin.x + size.x * 0.5f;
  const float rootY = origin.y + 70.0f;
  const float cardW = 90.0f;
  const float cardH = 46.0f;

  std::unordered_map<std::string, ImVec2> nodePos;
  nodePos[nodes[0].id] = ImVec2(rootX, rootY);

  if (nodes.size() > 1) {
    nodePos[nodes[1].id] = ImVec2(rootX - 120.0f, rootY + 90.0f);
  }
  if (nodes.size() > 2) {
    nodePos[nodes[2].id] = ImVec2(rootX + 120.0f, rootY + 90.0f);
  }

  // Draw Edges
  for (const auto &edge : scene.graph.edges) {
    auto fromIt = nodePos.find(edge.from_id);
    auto toIt = nodePos.find(edge.to_id);
    if (fromIt != nodePos.end() && toIt != nodePos.end()) {
      drawList->AddLine(fromIt->second, toIt->second, SolidCol::CardBorder, 2.0f);
    }
  }

  // Draw Nodes
  for (const auto &node : nodes) {
    auto it = nodePos.find(node.id);
    if (it == nodePos.end()) continue;

    ImVec2 center = it->second;
    ImVec2 minPt(center.x - cardW * 0.5f, center.y - cardH * 0.5f);
    ImVec2 maxPt(center.x + cardW * 0.5f, center.y + cardH * 0.5f);

    drawList->AddRectFilled(minPt, maxPt, SolidCol::CardBg, 6.0f);
    drawList->AddRect(minPt, maxPt, SolidCol::PrimaryLight, 6.0f, 0, 1.5f);

    ImVec2 tSize = ImGui::CalcTextSize(node.label.c_str());
    drawList->AddText(ImVec2(center.x - tSize.x * 0.5f, center.y - tSize.y * 0.5f),
                      SolidCol::TextWhite, node.label.c_str());

    // Cursors above node
    for (size_t cIdx = 0; cIdx < node.pointer_labels.size(); ++cIdx) {
      drawList->AddText(ImVec2(minPt.x, minPt.y - 18.0f), SolidCol::PrimaryLight,
                        node.pointer_labels[cIdx].c_str());
    }
  }
}

void AnimationEngine::renderGrid2DLayout(ImDrawList *drawList,
                                        const VisualScene &scene,
                                        ImVec2 origin, ImVec2 size,
                                        float progress) {
  (void)progress;
  int rows = scene.graph.grid_rows;
  int cols = scene.graph.grid_cols;
  if (rows <= 0 || cols <= 0) return;

  float maxGridW = size.x - 80.0f;
  float maxGridH = size.y - 80.0f;
  float cellW = std::clamp(maxGridW / static_cast<float>(cols), 32.0f, 64.0f);
  float cellH = std::clamp(maxGridH / static_cast<float>(rows), 28.0f, 48.0f);

  float totalW = static_cast<float>(cols) * cellW;
  float totalH = static_cast<float>(rows) * cellH;
  float startX = origin.x + (size.x - totalW) * 0.5f;
  float startY = origin.y + (size.y - totalH) * 0.5f + 10.0f;

  for (const auto &node : scene.graph.nodes) {
    // node.type is "[r][c]"
    int r = 0, c = 0;
    if (sscanf(node.type.c_str(), "[%d][%d]", &r, &c) == 2) {
      float cx = startX + static_cast<float>(c) * cellW;
      float cy = startY + static_cast<float>(r) * cellH;

      ImVec2 minPt(cx, cy);
      ImVec2 maxPt(cx + cellW - 4.0f, cy + cellH - 4.0f);

      ImU32 fillCol = node.is_mutated ? SolidCol::MutatedBg : SolidCol::CardBg;
      ImU32 borderCol = node.is_mutated ? SolidCol::MutatedBorder : SolidCol::CardBorder;

      drawList->AddRectFilled(minPt, maxPt, fillCol, 4.0f);
      drawList->AddRect(minPt, maxPt, borderCol, 4.0f, 0, 1.0f);

      ImVec2 tSize = ImGui::CalcTextSize(node.label.c_str());
      drawList->AddText(ImVec2(cx + (cellW - 4.0f - tSize.x) * 0.5f,
                               cy + (cellH - 4.0f - tSize.y) * 0.5f),
                        SolidCol::TextWhite, node.label.c_str());
    }
  }
}

void AnimationEngine::renderRecursionTreeLayout(ImDrawList *drawList,
                                                const VisualScene &scene,
                                                ImVec2 origin, ImVec2 size,
                                                float progress) {
  (void)progress;
  const auto &nodes = scene.graph.nodes;
  if (nodes.empty()) return;

  const float cardW = std::min(340.0f, size.x - 60.0f);
  const float cardH = 40.0f;
  const float spacing = 16.0f;
  float startY = origin.y + 50.0f;
  float startX = origin.x + (size.x - cardW) * 0.5f;

  for (size_t i = 0; i < nodes.size(); ++i) {
    const auto &node = nodes[i];
    float cy = startY + static_cast<float>(i) * (cardH + spacing);
    if (cy + cardH > origin.y + size.y) break;

    ImVec2 minPt(startX, cy);
    ImVec2 maxPt(startX + cardW, cy + cardH);

    ImU32 fillCol = node.is_mutated ? IM_COL32(37, 99, 235, 255) : SolidCol::CardBg;
    ImU32 borderCol = node.is_mutated ? SolidCol::PrimaryLight : SolidCol::CardBorder;

    drawList->AddRectFilled(minPt, maxPt, fillCol, 6.0f);
    drawList->AddRect(minPt, maxPt, borderCol, 6.0f, 0, node.is_mutated ? 2.0f : 1.0f);

    std::string text = "[" + node.type + "] " + node.label;
    drawList->AddText(ImVec2(startX + 12.0f, cy + 11.0f), SolidCol::TextWhite, text.c_str());

    // Connecting arrows down to caller
    if (i + 1 < nodes.size()) {
      float arrowX = startX + cardW * 0.5f;
      drawList->AddLine(ImVec2(arrowX, cy + cardH), ImVec2(arrowX, cy + cardH + spacing),
                        SolidCol::CardBorder, 1.5f);
    }
  }
}

void AnimationEngine::renderEmptyState(ImDrawList *drawList, ImVec2 origin, ImVec2 size) {
  drawList->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                          SolidCol::CanvasBg, 8.0f);
  drawList->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                    SolidCol::CardBorder, 8.0f, 0, 1.0f);

  const char *msg = "No data structure selected. Use [ ▾ Data Structures ] sidebar to choose one.";
  ImVec2 msgSize = ImGui::CalcTextSize(msg);
  ImVec2 msgPos(origin.x + (size.x - msgSize.x) * 0.5f,
                origin.y + (size.y - msgSize.y) * 0.5f);
  drawList->AddText(msgPos, SolidCol::TextMuted, msg);
}
