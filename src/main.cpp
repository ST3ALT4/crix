#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "compiler_helper.h"
#include "report.h"
#include "runtime_analyze.h"
#include "static_analyzer.h"
#include "visualizer/visualizer_app.h"

static void usage(const std::string &prog) {
  std::cerr
      << "Usage: " << prog
      << " --mode=<static|runtime|trace|both|visualize> [options] <file> [-- "
         "args...]\n\n"
      << "Modes:\n"
      << "  static      Parse source AST via libclang (variable types & "
         "scopes)\n"
      << "  runtime     Inspect runtime memory & values via DAP (summary "
         "table)\n"
      << "  trace       Record step-by-step timeline & array mutations for DSA "
         "visualizer\n"
      << "  both        Run step-by-step trace and display final summary "
         "table\n"
      << "  visualize   Launch Vulkan + Slang graphical visualizer window\n\n"
      << "Options:\n"
      << "  --format=<table|json>   Summary output format (default: table)\n"
      << "  --out=<path.json>       Export timeline trace to JSON file\n"
      << "  --max-steps=<N>         Maximum steps to trace (default: 1000)\n"
      << "  --interactive           Pause at each step in terminal\n"
      << "  --adapter=<path>        Path to DAP adapter (default: lldb-dap)\n\n"
      << "Examples:\n"
      << "  " << prog << " --mode=static tests/hello.c\n"
      << "  " << prog << " --mode=runtime tests/hello.c\n"
      << "  " << prog << " --mode=trace tests/bubble_sort.c --out=trace.json\n"
      << "  " << prog << " --mode=visualize tests/bubble_sort.c\n";
  std::exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    usage(argv[0]);
  }

  std::string mode;
  OutputFormat fmt = OutputFormat::TABLE;
  std::string file;
  std::string out_json_path;
  std::vector<std::string> extra;
  RuntimeConfig config;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg.rfind("--mode=", 0) == 0) {
      mode = arg.substr(7);
    } else if (arg.rfind("--format=", 0) == 0) {
      std::string f = arg.substr(9);
      if (f == "json") {
        fmt = OutputFormat::JSON;
      } else if (f == "table") {
        fmt = OutputFormat::TABLE;
      } else {
        std::cerr << "Unknown format: " << f << "\n";
        usage(argv[0]);
      }
    } else if (arg.rfind("--out=", 0) == 0) {
      out_json_path = arg.substr(6);
    } else if (arg.rfind("--max-steps=", 0) == 0) {
      config.max_steps = std::stoul(arg.substr(12));
    } else if (arg.rfind("--adapter=", 0) == 0) {
      config.adapter_path = arg.substr(10);
    } else if (arg == "--interactive") {
      config.interactive = true;
    } else if (arg == "--") {
      for (int j = i + 1; j < argc; j++) {
        extra.push_back(argv[j]);
      }
      break;
    } else if (arg[0] != '-') {
      file = arg;
    } else {
      std::cerr << "Unknown option: " << arg << "\n";
      usage(argv[0]);
    }
  }

  if (mode.empty()) {
    std::cerr << "Error: --mode required\n";
    usage(argv[0]);
  }
  if (file.empty()) {
    std::cerr << "Error: no input file\n";
    usage(argv[0]);
  }

  if (mode == "static") {
    VarList vl = static_analyze(file, extra);
    if (fmt == OutputFormat::JSON)
      report_json(vl, file);
    else
      report_table(vl, file);
    return EXIT_SUCCESS;
  }

  // Runtime, Trace, Both, or Visualize
  if (mode != "runtime" && mode != "trace" && mode != "both" &&
      mode != "visualize") {
    std::cerr << "Unknown mode: " << mode << "\n";
    usage(argv[0]);
  }

  // 1. Prepare target binary (auto-compile if source)
  CompiledTarget target = prepare_target(file);
  if (!target.success) {
    std::cerr << "Target preparation error: " << target.error_message << "\n";
    return EXIT_FAILURE;
  }

  // 2. If source file is available, run static analysis to seed the variable
  // table
  VarList static_vars;
  if (!target.source_path.empty()) {
    static_vars = static_analyze(target.source_path, {});
  }

  // 3. Run DAP analysis
  RuntimeResult res = runtime_analyze_dap(
      target.binary_path, target.source_path, extra, static_vars, config);

  if (!res.success) {
    std::cerr << "Runtime DAP analysis error: " << res.error_message << "\n";
    return EXIT_FAILURE;
  }

  // 4. Output A: Timeline / Trace
  if (mode == "trace" || mode == "both") {
    report_timeline(res.trace, config.interactive);
    if (!out_json_path.empty()) {
      if (export_trace_json(res.trace, out_json_path)) {
        std::cout << "Successfully exported trace JSON to: " << out_json_path
                  << "\n";
      }
    }
  }

  // 5. Output B: Summary Variable Table
  if (mode == "runtime" || mode == "both") {
    if (fmt == OutputFormat::JSON) {
      report_json(res.final_vars, file);
    } else {
      report_table(res.final_vars, file);
    }
  }

  // 6. Output C: Graphical Vulkan + Slang Visualizer
  if (mode == "visualize") {
    run_visualizer(res.trace);
  }

  return EXIT_SUCCESS;
}
