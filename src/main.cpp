#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "report.h"
#include "runtime_analyze.h"
#include "static_analyzer.h"

static void usage(const std::string &prog) {
  std::cerr << "Usage: " << prog
            << " --mode=<static|runtime> [--format=<table|json>]"
            << " <file> [-- args...]\n\n"
            << "Modes:\n"
            << "  static   Parse source via Clang AST (libclang)\n"
            << "           <file> must be a .c source file\n"
            << "           [-- clang-args] are forwarded to libclang\n"
            << "  runtime  GDB/LLDB will be used with the help of DAP\n"
            << "Formats:\n"
            << "  table    Human-readable aligned table (default)\n"
            << "  json     Machine-readable JSON array\n\n"
            << "Examples:\n"
            << "  " << prog << " --mode=static foo.c\n"
            << "  " << prog
            << " --mode=static --format=json foo.c -- -std=c11 -DDEBUG\n"
            << "  " << prog << " --mode=runtime ./a.out\n"
            << "  " << prog << " --mode=runtime ./a.out -- arg1 arg2\n";
  std::exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
  if (argc < 3) {
    usage(argv[0]);
  }

  std::string mode;
  OutputFormat fmt = OutputFormat::TABLE;
  std::string file;
  std::vector<std::string> extra;

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

  } else if (mode == "runtime") {
    runtime_analyze(file, extra);
  } else {
    std::cerr << "Unknown mode: " << mode << "\n";
    usage(argv[0]);
  }

  return EXIT_SUCCESS;
}
