#ifndef TOPOLOGY_DETECTOR_H
#define TOPOLOGY_DETECTOR_H

#include "trace_model.h"
#include <cstdint>
#include <string>
#include <vector>

// Universal Data Structure Topologies
enum class DSATopology {
  GENERIC_SCALAR = 0,
  SEQUENCE_1D,          // Array, vector, sliding window, two-pointer
  GRID_2D,              // 2D Array, Matrix, DP Table
  LINKED_LIST,          // Singly / Doubly linked list
  BINARY_TREE,          // Binary tree, BST, Binary heap
  GENERAL_GRAPH,        // Adjacency list / matrix
  RECURSION_TREE        // Backtracking / Call-stack state tree
};

struct TopologyNode {
  std::string id;            // Memory address, node index, or identifier
  std::string label;         // Display value / text
  std::string type;
  bool is_mutated = false;
  std::vector<std::string> pointer_labels; // Cursors pointing to this node (e.g. "head", "curr", "prev", "slow")
};

struct TopologyEdge {
  std::string from_id;
  std::string to_id;
  std::string label;         // e.g. "next", "prev", "left", "right"
  bool is_mutated = false;   // edge re-linked in active step
};

struct TopologyGraph {
  DSATopology topology = DSATopology::GENERIC_SCALAR;
  std::string description;   // Human-readable description (e.g., "Singly Linked List", "2D Grid / DP", "Sliding Window")
  std::vector<TopologyNode> nodes;
  std::vector<TopologyEdge> edges;

  // Topological layout metadata
  int grid_rows = 0;
  int grid_cols = 0;
  int64_t window_start = -1; // e.g. left / low
  int64_t window_end = -1;   // e.g. right / high
  int64_t focus_index = -1;  // e.g. mid / pivot
  int64_t compare_a = -1;    // active comparison pair index a
  int64_t compare_b = -1;    // active comparison pair index b
};

class TopologyDetector {
public:
  // Analyze active execution step and trace history to discover DSA topology
  static TopologyGraph discover(const StepRecord &step,
                                const ExecutionTrace &trace,
                                const std::string &targetVarName);
};

#endif // TOPOLOGY_DETECTOR_H
