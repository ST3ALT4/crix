#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "compiler_helper.h"
#include "lsp_analyzer.h"
#include "report.h"
#include "runtime_analyze.h"
#include "visualizer/visualizer_app.h"

static void usage(const std::string& prog) {
  std::cerr
      << "Usage: " << prog
      << " --mode=<trace|visualize> [options] <file> [-- args...]\n\n"
      << "Modes:\n"
      << "  trace       Compile, run step-by-step DAP trace, print timeline\n"
      << "  visualize   Compile, trace, launch Vulkan + Slang graphical window\n\n"
      << "Options:\n"
      << "  --format=<table|json>     Summary output format (default: table)\n"
      << "  --out=<path.json>         Export full trace JSON to file\n"
      << "  --max-steps=<N>           Max steps to trace (default: 1000)\n"
      << "  --interactive             Pause at each step in terminal\n"
      << "  --adapter=<path>          DAP adapter path (default: lldb-dap)\n"
      << "  --summary                 Print final variable summary table after trace\n"
      << "  --lsp-server=<cmd>        Language server command (auto-detected from\n"
      << "                            extension if omitted; e.g. clangd,\n"
      << "                            rust-analyzer, \"pyright --stdio\")\n"
      << "  --no-lsp                  Disable LSP analysis entirely\n\n"
      << "Examples:\n"
      << "  " << prog << " --mode=trace tests/bubble_sort.c --out=trace.json\n"
      << "  " << prog << " --mode=trace --summary tests/hello.c\n"
      << "  " << prog << " --mode=trace --lsp-server=clangd tests/nested.c\n"
      << "  " << prog << " --mode=trace --no-lsp tests/bubble_sort.c\n"
      << "  " << prog << " --mode=visualize tests/bubble_sort.c\n";
  std::exit(EXIT_FAILURE);
}

// Output format for --summary
enum class OutputFormat { TABLE, JSON };

int main(int argc, char** argv) {
  if (argc < 2) usage(argv[0]);

  std::string        mode;
  OutputFormat       fmt             = OutputFormat::TABLE;
  std::string        file;
  std::string        out_json_path;
  std::string        lsp_server_cmd;   // empty = auto-detect
  bool               no_lsp          = false;
  bool               show_summary    = false;
  std::vector<std::string> extra;
  RuntimeConfig      config;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if      (arg.rfind("--mode=",       0) == 0) { mode            = arg.substr(7);  }
    else if (arg.rfind("--format=",     0) == 0) {
      std::string f = arg.substr(9);
      if      (f == "json")  fmt = OutputFormat::JSON;
      else if (f == "table") fmt = OutputFormat::TABLE;
      else { std::cerr << "Unknown format: " << f << "\n"; usage(argv[0]); }
    }
    else if (arg.rfind("--out=",        0) == 0) { out_json_path   = arg.substr(6);  }
    else if (arg.rfind("--max-steps=",  0) == 0) { config.max_steps = std::stoul(arg.substr(12)); }
    else if (arg.rfind("--adapter=",    0) == 0) { config.adapter_path = arg.substr(10); }
    else if (arg.rfind("--lsp-server=", 0) == 0) { lsp_server_cmd  = arg.substr(13); }
    else if (arg == "--interactive")              { config.interactive = true;         }
    else if (arg == "--summary")                  { show_summary    = true;            }
    else if (arg == "--no-lsp")                   { no_lsp          = true;            }
    else if (arg == "--") {
      for (int j = i + 1; j < argc; ++j) extra.push_back(argv[j]);
      break;
    }
    else if (arg[0] != '-') { file = arg; }
    else { std::cerr << "Unknown option: " << arg << "\n"; usage(argv[0]); }
  }

  if (mode.empty()) { std::cerr << "Error: --mode required\n"; usage(argv[0]); }
  if (file.empty()) { std::cerr << "Error: no input file\n";   usage(argv[0]); }

  if (mode != "trace" && mode != "visualize") {
    std::cerr << "Unknown mode: " << mode
              << " (note: 'static', 'runtime', and 'both' modes have been removed)\n";
    usage(argv[0]);
  }

  // ── 1. LSP analysis (optional, non-fatal) ──────────────────────────────────
  SymbolList symbols;
  if (!no_lsp) {
    LspConfig lsp_cfg;
    lsp_cfg.server_cmd = lsp_server_cmd.empty()
                         ? detect_lsp_server(file)
                         : lsp_server_cmd;
    std::cerr << "[lsp] using server: " << lsp_cfg.server_cmd << "\n";
    symbols = lsp_analyze(file, lsp_cfg);
    if (symbols.empty()) {
      std::cerr << "[lsp] no symbols returned — continuing without LSP annotations\n";
    } else {
      std::cerr << "[lsp] " << symbols.size() << " symbol(s) extracted\n";
    }
  }

  // ── 2. Compile source → debug binary ───────────────────────────────────────
  CompiledTarget target = prepare_target(file);
  if (!target.success) {
    std::cerr << "Target preparation error: " << target.error_message << "\n";
    return EXIT_FAILURE;
  }

  // ── 3. DAP runtime trace ────────────────────────────────────────────────────
  RuntimeResult res = runtime_analyze_dap(
      target.binary_path, target.source_path, extra, config);

  if (!res.success) {
    std::cerr << "Runtime DAP analysis error: " << res.error_message << "\n";
    return EXIT_FAILURE;
  }

  // ── 4. Annotate trace with LSP scope metadata ───────────────────────────────
  if (!symbols.empty()) {
    annotate_trace(res.trace, symbols);
  }

  // ── 5. Output ───────────────────────────────────────────────────────────────
  if (mode == "trace") {
    report_timeline(res.trace, config.interactive);

    if (!out_json_path.empty()) {
      if (export_trace_json(res.trace, out_json_path)) {
        std::cout << "Exported trace JSON to: " << out_json_path << "\n";
      }
    }

    if (show_summary) {
      if (fmt == OutputFormat::JSON) report_summary_json(res.trace, file);
      else                           report_summary_table(res.trace, file);
    }
  }

  if (mode == "visualize") {
    run_visualizer(res.trace);
  }

  return EXIT_SUCCESS;
}
