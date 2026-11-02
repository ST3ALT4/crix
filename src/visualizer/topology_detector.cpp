#include "visualizer/topology_detector.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

static std::string toLower(const std::string &s) {
  std::string res = s;
  for (char &c : res) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return res;
}

TopologyGraph TopologyDetector::discover(const StepRecord &step,
                                        const ExecutionTrace &trace,
                                        const std::string &targetVarName) {
  (void)trace;
  TopologyGraph g;
  g.topology = DSATopology::GENERIC_SCALAR;

  if (step.variables.empty()) {
    return g;
  }

  // 1. Check for Recursion Call Tree if call stack has repeated function invocations
  int recursionDepth = 0;
  std::string recFunc;
  for (size_t i = 0; i < step.call_stack.size(); ++i) {
    for (size_t j = i + 1; j < step.call_stack.size(); ++j) {
      if (!step.call_stack[i].name.empty() &&
          step.call_stack[i].name == step.call_stack[j].name) {
        recursionDepth++;
        recFunc = step.call_stack[i].name;
      }
    }
  }

  // If user explicitly chose or if strong recursion is occurring and no collection was chosen:
  if (recursionDepth >= 1 && (targetVarName == "call_stack" || targetVarName.empty())) {
    bool hasCollection = false;
    for (const auto &v : step.variables) {
      if (v.second.kind == ValueKind::ARRAY && !v.second.elements.empty()) {
        hasCollection = true;
        break;
      }
    }

    if (!hasCollection || targetVarName == "call_stack") {
      g.topology = DSATopology::RECURSION_TREE;
      g.description = "Recursion Tree (" + recFunc + "(), depth " +
                      std::to_string(step.call_stack.size()) + ")";
      for (size_t i = 0; i < step.call_stack.size(); ++i) {
        const auto &frame = step.call_stack[i];
        TopologyNode node;
        node.id = std::to_string(i);
        node.type = "Frame " + std::to_string(i);
        std::string summary = frame.name + "()";
        if (!frame.variables.empty()) {
          summary += " [";
          for (size_t vi = 0; vi < frame.variables.size() && vi < 3; ++vi) {
            if (vi > 0) summary += ", ";
            summary += frame.variables[vi].first + "=" + frame.variables[vi].second.to_string();
          }
          summary += "]";
        }
        node.label = summary;
        node.is_mutated = (i == 0);
        g.nodes.push_back(node);

        if (i + 1 < step.call_stack.size()) {
          TopologyEdge edge;
          edge.from_id = std::to_string(i + 1);
          edge.to_id = std::to_string(i);
          edge.label = "calls";
          g.edges.push_back(edge);
        }
      }
      return g;
    }
  }

  // 2. Select target variable
  const std::pair<std::string, RuntimeValue> *chosen = nullptr;
  if (!targetVarName.empty()) {
    for (const auto &v : step.variables) {
      if (v.first == targetVarName) {
        chosen = &v;
        break;
      }
    }
  }

  if (!chosen) {
    // Priority: Arrays -> Linked list / Tree structs -> Pointers -> Scalars
    for (const auto &v : step.variables) {
      if (v.second.kind == ValueKind::ARRAY && !v.second.elements.empty()) {
        chosen = &v;
        break;
      }
    }
  }
  if (!chosen) {
    for (const auto &v : step.variables) {
      if (v.second.kind == ValueKind::POINTER || v.second.kind == ValueKind::STRUCT) {
        chosen = &v;
        break;
      }
    }
  }
  if (!chosen && !step.variables.empty()) {
    chosen = &step.variables[0];
  }

  if (!chosen) return g;

  const std::string &varName = chosen->first;
  const RuntimeValue &val = chosen->second;

  // 3. Check for 2D Grid / Matrix / DP Table
  if (val.kind == ValueKind::ARRAY && !val.elements.empty() &&
      val.elements[0].kind == ValueKind::ARRAY) {
    g.topology = DSATopology::GRID_2D;
    g.grid_rows = static_cast<int>(val.elements.size());
    g.grid_cols = static_cast<int>(val.elements[0].elements.size());
    g.description = "2D Grid / Matrix (" + std::to_string(g.grid_rows) + "x" +
                    std::to_string(g.grid_cols) + ")";

    // Check row / col iterators
    for (const auto &v : step.variables) {
      if (v.first == "r" || v.first == "row" || v.first == "i") {
        try { g.window_start = std::stoll(v.second.raw_value); } catch (...) {}
      }
      if (v.first == "c" || v.first == "col" || v.first == "j") {
        try { g.window_end = std::stoll(v.second.raw_value); } catch (...) {}
      }
    }

    for (int r = 0; r < g.grid_rows; ++r) {
      for (int c = 0; c < g.grid_cols; ++c) {
        TopologyNode node;
        node.id = std::to_string(r) + "," + std::to_string(c);
        if (c < static_cast<int>(val.elements[r].elements.size())) {
          node.label = val.elements[r].elements[c].to_string();
        } else {
          node.label = "0";
        }
        node.type = "[" + std::to_string(r) + "][" + std::to_string(c) + "]";
        node.is_mutated = (r == g.window_start && c == g.window_end);
        g.nodes.push_back(node);
      }
    }
    return g;
  }

  // 4. Check for Linked List or Binary Tree node-link topology
  bool isLinkedList = false;
  bool isBinaryTree = false;

  auto inspectFields = [&](const std::vector<std::pair<std::string, RuntimeValue>> &fields) {
    bool hasNext = false, hasPrev = false, hasLeft = false, hasRight = false;
    for (const auto &f : fields) {
      std::string lName = toLower(f.first);
      if (lName == "next") hasNext = true;
      if (lName == "prev") hasPrev = true;
      if (lName == "left") hasLeft = true;
      if (lName == "right") hasRight = true;
    }
    if (hasLeft && hasRight) isBinaryTree = true;
    else if (hasNext || hasPrev) isLinkedList = true;
  };

  if (val.kind == ValueKind::STRUCT) {
    inspectFields(val.fields);
  } else if (val.kind == ValueKind::POINTER && !val.fields.empty()) {
    inspectFields(val.fields);
  }

  // If node-link detected:
  if (isBinaryTree) {
    g.topology = DSATopology::BINARY_TREE;
    g.description = "Binary Tree / BST (Node: " + varName + ")";

    TopologyNode root;
    root.id = varName;
    root.type = val.type_name;
    root.label = varName;
    for (const auto &f : val.fields) {
      if (toLower(f.first) == "val" || toLower(f.first) == "data" || toLower(f.first) == "key") {
        root.label = f.second.to_string();
        break;
      }
    }
    root.pointer_labels.push_back(varName);
    g.nodes.push_back(root);

    // Add children if present in fields
    for (const auto &f : val.fields) {
      std::string lName = toLower(f.first);
      if ((lName == "left" || lName == "right") && !f.second.raw_value.empty() &&
          f.second.raw_value != "0x0" && f.second.raw_value != "NULL") {
        TopologyNode child;
        child.id = f.second.raw_value;
        child.label = lName + " (" + f.second.raw_value.substr(0, 6) + ")";
        child.type = f.second.type_name;
        g.nodes.push_back(child);

        TopologyEdge edge;
        edge.from_id = root.id;
        edge.to_id = child.id;
        edge.label = lName;
        g.edges.push_back(edge);
      }
    }
    return g;
  }

  if (isLinkedList) {
    g.topology = DSATopology::LINKED_LIST;
    g.description = "Linked List (Head: " + varName + ")";

    TopologyNode node;
    node.id = varName;
    node.type = val.type_name;
    node.label = varName;
    for (const auto &f : val.fields) {
      if (toLower(f.first) == "val" || toLower(f.first) == "data" || toLower(f.first) == "value") {
        node.label = f.second.to_string();
        break;
      }
    }
    node.pointer_labels.push_back(varName);

    // Look for other pointer cursors (curr, prev, slow, fast, head)
    for (const auto &v : step.variables) {
      if (v.first != varName && v.second.kind == ValueKind::POINTER) {
        if (v.second.raw_value == val.raw_value && !val.raw_value.empty()) {
          node.pointer_labels.push_back(v.first);
        }
      }
    }
    g.nodes.push_back(node);

    // Trace next pointer
    for (const auto &f : val.fields) {
      if (toLower(f.first) == "next" && !f.second.raw_value.empty() &&
          f.second.raw_value != "0x0" && f.second.raw_value != "NULL") {
        TopologyNode nextNode;
        nextNode.id = f.second.raw_value;
        nextNode.label = "next (" + f.second.raw_value.substr(0, 6) + ")";
        nextNode.type = f.second.type_name;
        g.nodes.push_back(nextNode);

        TopologyEdge edge;
        edge.from_id = node.id;
        edge.to_id = nextNode.id;
        edge.label = "next";
        g.edges.push_back(edge);
        break;
      }
    }
    return g;
  }

  // 5. Sequence 1D / Multi-Pointer Window / Search & Sorting
  if (val.kind == ValueKind::ARRAY) {
    g.topology = DSATopology::SEQUENCE_1D;
    const size_t count = val.elements.size();

    // Check pointer indices (left, right, mid, low, high, pivot, i, j)
    int64_t leftVal = -1, rightVal = -1, midVal = -1, iVal = -1, jVal = -1;

    for (const auto &v : step.variables) {
      std::string lName = toLower(v.first);
      if (v.second.kind == ValueKind::PRIMITIVE) {
        try {
          int64_t num = std::stoll(v.second.raw_value);
          if (lName == "left" || lName == "low") leftVal = num;
          else if (lName == "right" || lName == "high") rightVal = num;
          else if (lName == "mid" || lName == "pivot") midVal = num;
          else if (lName == "i") iVal = num;
          else if (lName == "j") jVal = num;
        } catch (...) {}
      }
    }

    if (leftVal != -1 && rightVal != -1) {
      g.window_start = std::clamp<int64_t>(leftVal, 0, count > 0 ? count - 1 : 0);
      g.window_end = std::clamp<int64_t>(rightVal, 0, count > 0 ? count - 1 : 0);
      g.focus_index = midVal;
      g.description = "Converging Search Window [" + std::to_string(g.window_start) +
                      ".." + std::to_string(g.window_end) + "]";
    } else if (iVal != -1 && jVal != -1) {
      g.compare_a = iVal;
      g.compare_b = jVal;
      g.description = "Multi-Pointer Sequence (i=" + std::to_string(iVal) +
                      ", j=" + std::to_string(jVal) + ")";
    } else {
      g.description = "Sequence Buffer (" + std::to_string(count) + " elements)";
    }

    // Identify array mutations in this step
    std::unordered_set<size_t> mutatedIndices;
    for (const auto &mut : step.delta.array_mutations) {
      if (mut.variable_name == varName) {
        mutatedIndices.insert(mut.index);
      }
    }

    for (size_t idx = 0; idx < count; ++idx) {
      TopologyNode node;
      node.id = std::to_string(idx);
      node.label = val.elements[idx].to_string();
      node.type = "[" + std::to_string(idx) + "]";
      node.is_mutated = (mutatedIndices.count(idx) > 0);

      // Attach pointer labels
      if (static_cast<int64_t>(idx) == leftVal) node.pointer_labels.push_back("left");
      if (static_cast<int64_t>(idx) == rightVal) node.pointer_labels.push_back("right");
      if (static_cast<int64_t>(idx) == midVal) node.pointer_labels.push_back("mid");
      if (static_cast<int64_t>(idx) == iVal) node.pointer_labels.push_back("i");
      if (static_cast<int64_t>(idx) == jVal) node.pointer_labels.push_back("j");

      g.nodes.push_back(node);
    }
    return g;
  }

  // 6. Fallback: Generic Scalar or Single Struct
  g.topology = DSATopology::GENERIC_SCALAR;
  g.description = "Scalar / Record (" + varName + ")";
  TopologyNode node;
  node.id = varName;
  node.label = val.to_string();
  node.type = val.type_name;
  node.is_mutated = std::find(step.delta.modified_vars.begin(),
                              step.delta.modified_vars.end(),
                              varName) != step.delta.modified_vars.end();
  g.nodes.push_back(node);

  return g;
}
