#ifndef TRACE_VISUAL_ADAPTER_H
#define TRACE_VISUAL_ADAPTER_H

#include "trace_model.h"
#include "visualizer/visual_model.h"
#include <string>
#include <vector>

// Metadata for a data structure candidate discoverable in the active execution step
struct DataStructureCandidate {
  std::string name;
  std::string type;
  std::string scope_name;
  VisualNodeKind kind;
  size_t element_count = 0;
  bool is_active_in_step = false;
};

// Bridge adapter: translates CRIX-specific execution step records into generic VisualScenes
class TraceVisualAdapter {
public:
  // Convert a CRIX StepRecord into an independent, modular VisualScene
  static VisualScene buildVisualScene(const StepRecord &step,
                                      const std::string &targetVarName,
                                      float animationProgress);

  // Discover all variables / data structures available in the current step
  static std::vector<DataStructureCandidate>
  discoverDataStructures(const StepRecord &step, const ExecutionTrace &trace);
};

#endif // TRACE_VISUAL_ADAPTER_H
