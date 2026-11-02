#include "visualizer/trace_visual_adapter.h"
#include <algorithm>
#include <unordered_set>

VisualScene
TraceVisualAdapter::buildVisualScene(const StepRecord &step,
                                     const std::string &targetVarName,
                                     float animationProgress) {
  (void)animationProgress;
  VisualScene scene;
  scene.is_empty = true;

  if (step.variables.empty()) {
    return scene;
  }

  // Find target variable or pick best candidate if targetVarName is empty
  const std::pair<std::string, RuntimeValue> *chosen = nullptr;
  if (!targetVarName.empty()) {
    for (const auto &v : step.variables) {
      if (v.first == targetVarName) {
        chosen = &v;
        break;
      }
    }
  }

  // Fallback heuristic: choose first array, struct, or pointer
  if (!chosen) {
    for (const auto &v : step.variables) {
      if (v.second.kind == ValueKind::ARRAY && !v.second.elements.empty()) {
        chosen = &v;
        break;
      }
    }
  }
  if (!chosen) {
    for (const auto &v : step.variables) {
      if (v.second.kind == ValueKind::STRUCT || v.second.kind == ValueKind::POINTER) {
        chosen = &v;
        break;
      }
    }
  }
  if (!chosen && !step.variables.empty()) {
    chosen = &step.variables[0];
  }

  if (!chosen) {
    return scene;
  }

  const std::string &varName = chosen->first;
  const RuntimeValue &val = chosen->second;

  scene.title = varName;
  scene.type_name = val.type_name;
  scene.is_empty = false;
  scene.graph = TopologyDetector::discover(step, ExecutionTrace{}, targetVarName);

  // Build modified vars lookup
  std::unordered_set<std::string> modVars(step.delta.modified_vars.begin(),
                                          step.delta.modified_vars.end());

  switch (val.kind) {
  case ValueKind::ARRAY: {
    // Array: build sequence of ARRAY_CELL nodes
    std::unordered_set<size_t> mutatedIndices;
    for (const auto &mut : step.delta.array_mutations) {
      if (mut.variable_name == varName) {
        mutatedIndices.insert(mut.index);
      }
    }

    const size_t count = val.elements.size();
    for (size_t i = 0; i < count; ++i) {
      VisualNode node;
      node.id = static_cast<int64_t>(i);
      node.label = "[" + std::to_string(i) + "]";
      node.value = val.elements[i].to_string();
      node.type = val.elements[i].type_name;
      node.kind = VisualNodeKind::ARRAY_CELL;
      node.is_mutated = (mutatedIndices.find(i) != mutatedIndices.end());
      scene.nodes.push_back(std::move(node));
    }

    // Extract loop iterators / indices (e.g. i, j, k, left, right, mid)
    for (const auto &v : step.variables) {
      if (v.first == varName) continue;
      if (v.second.kind == ValueKind::PRIMITIVE &&
          (v.second.type_name == "int" || v.second.type_name == "size_t" ||
           v.second.type_name == "unsigned int") &&
          v.second.type_name.find('*') == std::string::npos) {
        try {
          int idxVal = std::stoi(v.second.raw_value);
          if (idxVal >= 0 && static_cast<size_t>(idxVal) < count) {
            VisualPointer ptr;
            ptr.label = v.first;
            ptr.target_index = idxVal;
            scene.pointers.push_back(ptr);
          }
        } catch (...) {}
      }
    }
    break;
  }

  case ValueKind::STRUCT: {
    // Struct: build composite STRUCT_RECORD node with properties
    VisualNode node;
    node.id = 0;
    node.label = varName;
    node.type = val.type_name;
    node.kind = VisualNodeKind::STRUCT_RECORD;
    node.is_mutated = (modVars.find(varName) != modVars.end());

    for (const auto &f : val.fields) {
      VisualProperty prop;
      prop.name = f.first;
      prop.value = f.second.to_string();
      prop.type = f.second.type_name;
      prop.is_mutated = node.is_mutated;
      node.properties.push_back(std::move(prop));
    }
    scene.nodes.push_back(std::move(node));
    break;
  }

  case ValueKind::POINTER: {
    // Pointer: build POINTER_LINK node
    VisualNode node;
    node.id = 0;
    node.label = varName;
    node.value = val.raw_value.empty() ? val.to_string() : val.raw_value;
    node.type = val.type_name;
    node.kind = VisualNodeKind::POINTER_LINK;
    node.is_mutated = (modVars.find(varName) != modVars.end());
    scene.nodes.push_back(std::move(node));
    break;
  }

  case ValueKind::PRIMITIVE:
  default: {
    // Scalar or collection: build SCALAR node
    VisualNode node;
    node.id = 0;
    node.label = varName;
    node.value = val.to_string();
    node.type = val.type_name;
    node.kind = VisualNodeKind::SCALAR;
    node.is_mutated = (modVars.find(varName) != modVars.end());
    scene.nodes.push_back(std::move(node));
    break;
  }
  }

  return scene;
}

std::vector<DataStructureCandidate>
TraceVisualAdapter::discoverDataStructures(const StepRecord &step,
                                           const ExecutionTrace &trace) {
  std::vector<DataStructureCandidate> candidates;
  std::unordered_set<std::string> seen;

  // 1. First add variables from active step
  for (const auto &v : step.variables) {
    if (seen.insert(v.first).second) {
      DataStructureCandidate c;
      c.name = v.first;
      c.type = v.second.type_name;
      c.is_active_in_step = true;

      auto scIt = step.var_scope_names.find(v.first);
      if (scIt != step.var_scope_names.end()) {
        c.scope_name = scIt->second;
      }

      if (v.second.kind == ValueKind::ARRAY) {
        c.kind = VisualNodeKind::ARRAY_CELL;
        c.element_count = v.second.elements.size();
      } else if (v.second.kind == ValueKind::STRUCT) {
        c.kind = VisualNodeKind::STRUCT_RECORD;
        c.element_count = v.second.fields.size();
      } else if (v.second.kind == ValueKind::POINTER) {
        c.kind = VisualNodeKind::POINTER_LINK;
      } else {
        c.kind = VisualNodeKind::SCALAR;
      }
      candidates.push_back(c);
    }
  }

  // 2. Add other candidate structures appearing elsewhere in the trace
  for (const auto &s : trace.steps) {
    for (const auto &v : s.variables) {
      if (seen.insert(v.first).second) {
        DataStructureCandidate c;
        c.name = v.first;
        c.type = v.second.type_name;
        c.is_active_in_step = false;

        auto scIt = s.var_scope_names.find(v.first);
        if (scIt != s.var_scope_names.end()) {
          c.scope_name = scIt->second;
        }

        if (v.second.kind == ValueKind::ARRAY) {
          c.kind = VisualNodeKind::ARRAY_CELL;
          c.element_count = v.second.elements.size();
        } else if (v.second.kind == ValueKind::STRUCT) {
          c.kind = VisualNodeKind::STRUCT_RECORD;
          c.element_count = v.second.fields.size();
        } else if (v.second.kind == ValueKind::POINTER) {
          c.kind = VisualNodeKind::POINTER_LINK;
        } else {
          c.kind = VisualNodeKind::SCALAR;
        }
        candidates.push_back(c);
      }
    }
  }

  return candidates;
}
