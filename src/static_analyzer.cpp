#include <iostream>
#include <vector>
#include <string>

#include <clang-c/Index.h>

#include "ast_visitor.h"
#include "static_analyzer.h"

VarList static_analyze(const std::string &filepath, const std::vector<std::string> &extra_args) {
  CXIndex idx = clang_createIndex(0, 0);
  if (!idx) {
    std::cerr << "clang_createIndex failed\n";
    return {};
  }

  std::vector<const char *> clang_argv;
  clang_argv.reserve(extra_args.size());
  for (const auto &arg : extra_args) {
    clang_argv.push_back(arg.c_str());
  }

  /* Parse the file */
  CXTranslationUnit tu = clang_parseTranslationUnit(
      idx, filepath.c_str(), clang_argv.data(), static_cast<int>(clang_argv.size()),
      nullptr, 0,
      CXTranslationUnit_DetailedPreprocessingRecord
  );

  if (!tu) {
    std::cerr << "Failed to parse: " << filepath << "\n";
    clang_disposeIndex(idx);
    return {};
  }

  /* Report diagnostics */
  unsigned int num_diag = clang_getNumDiagnostics(tu);
  for (unsigned int i = 0; i < num_diag; i++) {
    CXDiagnostic d = clang_getDiagnostic(tu, i);
    CXString ds = clang_formatDiagnostic(d, CXDiagnostic_DisplaySourceLocation |
                                                 CXDiagnostic_DisplayOption);
    std::cerr << "diag: " << clang_getCString(ds) << "\n";
    clang_disposeString(ds);
    clang_disposeDiagnostic(d);
  }

  VarList vl;
  ast_visit(tu, vl);

  clang_disposeTranslationUnit(tu);
  clang_disposeIndex(idx);
  return vl;
}
