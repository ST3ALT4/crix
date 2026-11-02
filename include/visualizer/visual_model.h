#ifndef VISUAL_MODEL_H
#define VISUAL_MODEL_H

#include <cstdint>
#include <string>
#include <vector>
#include "visualizer/topology_detector.h"

// Generic node kinds supported by the modular visualizer
enum class VisualNodeKind {
  SCALAR,
  ARRAY_CELL,
  STRUCT_RECORD,
  POINTER_LINK,
  CUSTOM
};

// Key-value property for composite types (e.g. struct fields)
struct VisualProperty {
  std::string name;
  std::string value;
  std::string type;
  bool is_mutated = false;
};

// Pointer / iterator indicator (e.g. loop index 'i' or 'j', or pointer arrow)
struct VisualPointer {
  std::string label;        // e.g. "i", "j", "head"
  int64_t target_index = 0; // index in array or target node ID
  uint32_t color_rgba = 0;  // solid color: IM_COL32 / 0xRRGGBBAA
};

// Generic visual node representation
struct VisualNode {
  int64_t id = 0;
  std::string label;
  std::string value;
  std::string type;
  VisualNodeKind kind = VisualNodeKind::SCALAR;
  std::vector<VisualProperty> properties; // for structs / records
  bool is_mutated = false;
  float mutation_pulse = 0.0f;
};

// A complete decoupled visual scene ready for rendering
struct VisualScene {
  std::string title;
  std::string type_name;
  std::string narration;
  TopologyGraph graph;
  std::vector<VisualNode> nodes;
  std::vector<VisualPointer> pointers;
  bool is_empty = true;
};

#endif // VISUAL_MODEL_H
