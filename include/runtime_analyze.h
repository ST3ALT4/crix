#ifndef RUNTIME_ANALYZE_H
#define RUNTIME_ANALYZE_H

#include <string>
#include <vector>
#include "static_analyzer.h"
#include "trace_model.h"

struct RuntimeConfig {
  std::string adapter_path = "lldb-dap";
  size_t max_steps = 1000;
  bool step_into_user_functions = true;
  std::string json_output_path;
  bool interactive = false;
};

struct RuntimeResult {
  ExecutionTrace trace;
  VarList final_vars;
  bool success = false;
  std::string error_message;
};

RuntimeResult runtime_analyze_dap(
    const std::string &binary_path,
    const std::string &source_file,
    const std::vector<std::string> &prog_args,
    const VarList &static_vars,
    const RuntimeConfig &config = RuntimeConfig{}
);

#endif // RUNTIME_ANALYZE_H
