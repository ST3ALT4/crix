#ifndef TRACE_MODEL_H
#define TRACE_MODEL_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

enum class ValueKind {
  PRIMITIVE,
  ARRAY,
  STRUCT,
  POINTER,
  OTHER
};

struct RuntimeValue {
  ValueKind kind = ValueKind::PRIMITIVE;
  std::string type_name;
  std::string raw_value;
  std::vector<RuntimeValue> elements; // for arrays
  std::vector<std::pair<std::string, RuntimeValue>> fields; // for structs

  std::string to_string() const;
};

struct ArrayMutation {
  std::string variable_name;
  size_t index = 0;
  std::string prev_value;
  std::string curr_value;
};

struct StepDelta {
  std::vector<std::string> modified_vars;
  std::vector<ArrayMutation> array_mutations;
};

struct StackFrameInfo {
  int64_t id = 0;
  std::string name;
  std::string file;
  int line = 0;
  int col = 0;
  std::vector<std::pair<std::string, RuntimeValue>> variables;
};

struct StepRecord {
  int step_number = 0;
  std::string file;
  int line = 0;
  int col = 0;
  std::string function_name;
  std::vector<StackFrameInfo> call_stack;
  std::vector<std::pair<std::string, RuntimeValue>> variables;
  StepDelta delta;
  std::string stdout_chunk;
};

struct ExecutionTrace {
  std::string source_file;
  std::vector<StepRecord> steps;

  std::string to_json(bool pretty = true) const;
};

// Compute state mutations between prev_step and curr_step
StepDelta compute_delta(const StepRecord &prev_step, const StepRecord &curr_step);

#endif // TRACE_MODEL_H
