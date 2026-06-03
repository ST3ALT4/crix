#ifndef STATIC_ANALYZER_H
#define STATIC_ANALYZER_H

#include <string>
#include <vector>

enum class OutputFormat {
  TABLE,
  JSON
};

enum class ScopeKind {
  GLOBAL,
  FUNCTION,
  BLOCK,
  STRUCT,
  ENUM
};

struct VarInfo {
  std::string name;
  std::string type;
  std::string scope_name;
  ScopeKind scope_kind;
  unsigned int line;
  unsigned int col;
  std::string value;
};

using VarList = std::vector<VarInfo>;

VarList static_analyze(const std::string &filepath, const std::vector<std::string> &extra_args);

#endif
