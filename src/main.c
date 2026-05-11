#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "report.h"
#include "runtime_analyzer.h"
#include "static_analyzer.h"

static void usage(const char *prog) {
  fprintf(stderr,
          "Usage: %s --mode=<static|runtime> [--format=<table|json>]"
          " <file> [-- args...]\n"
          "\n"
          "Modes:\n"
          "  static   Parse source via Clang AST (libclang)\n"
          "           <file> must be a .c source file\n"
          "           [-- clang-args] are forwarded to libclang\n"
          "  runtime  ptrace + DWARF live variable inspection\n"
          "           <file> must be an ELF binary compiled with -g -O0\n"
          "           [-- prog-args] are forwarded to the traced binary\n"
          "\n"
          "Formats:\n"
          "  table    Human-readable aligned table (default)\n"
          "  json     Machine-readable JSON array\n"
          "\n"
          "Examples:\n"
          "  %s --mode=static foo.c\n"
          "  %s --mode=static --format=json foo.c -- -std=c11 -DDEBUG\n"
          "  %s --mode=runtime ./a.out\n"
          "  %s --mode=runtime ./a.out -- arg1 arg2\n",
          prog, prog, prog, prog, prog);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
  if (argc < 3)
    usage(argv[0]);

  const char *mode = NULL;
  OutputFormat fmt = FMT_TABLE;
  const char *file = NULL;
  char **extra = NULL;
  int extra_argc = 0;

  for (int i = 1; i < argc; i++) {
    if (strncmp(argv[i], "--mode=", 7) == 0) {
      mode = argv[i] + 7;
    } else if (strncmp(argv[i], "--format=", 9) == 0) {
      const char *f = argv[i] + 9;
      if (strcmp(f, "json") == 0)
        fmt = FMT_JSON;
      else if (strcmp(f, "table") == 0)
        fmt = FMT_TABLE;
      else {
        fprintf(stderr, "Unknown format: %s\n", f);
        usage(argv[0]);
      }
    } else if (strcmp(argv[i], "--") == 0) {
      extra = argv + i + 1;
      extra_argc = argc - i - 1;
      break;
    } else if (argv[i][0] != '-') {
      file = argv[i];
    } else {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      usage(argv[0]);
    }
  }

  if (!mode) {
    fputs("Error: --mode required\n", stderr);
    usage(argv[0]);
  }
  if (!file) {
    fputs("Error: no input file\n", stderr);
    usage(argv[0]);
  }

  if (strcmp(mode, "static") == 0) {
    VarList *vl = static_analyze(file, (const char **)extra, extra_argc);
    if (!vl) {
      fprintf(stderr, "Analysis failed for: %s\n", file);
      return EXIT_FAILURE;
    }
    if (fmt == FMT_JSON)
      report_json(vl, file);
    else
      report_table(vl, file);
    varlist_free(vl);

  } else if (strcmp(mode, "runtime") == 0) {
    int rt_argc = 1 + extra_argc;
    char **rt_argv = calloc((size_t)(rt_argc + 1), sizeof *rt_argv);
    if (!rt_argv) {
      perror("calloc");
      return EXIT_FAILURE;
    }
    rt_argv[0] = (char *)file;
    for (int i = 0; i < extra_argc; i++)
      rt_argv[1 + i] = extra[i];
    rt_argv[rt_argc] = NULL;

    VarList *vl = runtime_analyze(file, rt_argv);
    free(rt_argv);

    if (!vl) {
      fprintf(stderr, "Runtime analysis failed for: %s\n", file);
      return EXIT_FAILURE;
    }
    if (fmt == FMT_JSON)
      report_json(vl, file);
    else
      report_table(vl, file);
    varlist_free(vl);

  } else {
    fprintf(stderr, "Unknown mode: %s\n", mode);
    usage(argv[0]);
  }

  return EXIT_SUCCESS;
}
