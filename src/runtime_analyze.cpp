#include "runtime_analyze.h"
#include "dap/io.h"
#include <string>
#include <vector>

void runtime_analyze(const std::string &binary_path,
                     const std::vector<std::string> &args) {
  (void)binary_path;
  (void)args;
  // Will implement runtime analysis using DAP later on using google cppdap
}
