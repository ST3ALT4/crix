#include <iostream>
#include <string>
#include <vector>
#include <cstdio>

#include "report.h"
#include "static_analyzer.h"

static const char *scope_str(ScopeKind k) {
  switch (k) {
  case ScopeKind::GLOBAL:
    return "global";
  case ScopeKind::FUNCTION:
    return "function";
  case ScopeKind::BLOCK:
    return "block";
  case ScopeKind::STRUCT:
    return "struct";
  case ScopeKind::ENUM:
    return "enum";
  }
  return "?";
}

void report_table(const VarList &vl, const std::string &filepath) {
  /* Detect whether any entry has a non-empty value (runtime mode) */
  int has_values = 0;
  for (const auto &var : vl) {
    if (!var.value.empty()) {
      has_values = 1;
      break;
    }
  }

  const char *mode_label = has_values ? "Runtime" : "Static";
  std::printf("\nVarScope — %s Analysis: %s\n", mode_label, filepath.c_str());
  std::printf("Variables found: %zu\n\n", vl.size());

  /* column widths */
  int w_name = 12, w_type = 20, w_scope = 16, w_skind = 10, w_val = 14;

  for (const auto &var : vl) {
    int n;
    if ((n = static_cast<int>(var.name.length())) > w_name)
      w_name = n;
    if ((n = static_cast<int>(var.type.length())) > w_type)
      w_type = n;
    if ((n = static_cast<int>(var.scope_name.length())) > w_scope)
      w_scope = n;
    if (has_values && (n = static_cast<int>(var.value.length())) > w_val)
      w_val = n;
  }

  /* header */
  if (has_values)
    std::printf("%-*s  %-*s  %-*s  %-*s  %5s  %4s  %-*s\n", w_name, "NAME", w_type,
           "TYPE", w_scope, "SCOPE", w_skind, "SCOPE-KIND", "LINE", "COL",
           w_val, "VALUE");
  else
    std::printf("%-*s  %-*s  %-*s  %-*s  %5s  %4s\n", w_name, "NAME", w_type, "TYPE",
           w_scope, "SCOPE", w_skind, "SCOPE-KIND", "LINE", "COL");

  int sep_len =
      w_name + w_type + w_scope + w_skind + 18 + (has_values ? w_val + 2 : 0);
  for (int i = 0; i < sep_len; i++)
    std::putchar('-');
  std::putchar('\n');

  for (const auto &v : vl) {
    if (has_values)
      std::printf("%-*s  %-*s  %-*s  %-*s  %5u  %4u  %-*s\n", w_name, v.name.c_str(),
             w_type, v.type.c_str(), w_scope, v.scope_name.c_str(), w_skind,
             scope_str(v.scope_kind), v.line, v.col, w_val, v.value.c_str());
    else
      std::printf("%-*s  %-*s  %-*s  %-*s  %5u  %4u\n", w_name, v.name.c_str(), w_type,
             v.type.c_str(), w_scope, v.scope_name.c_str(), w_skind, scope_str(v.scope_kind),
             v.line, v.col);
  }
  std::putchar('\n');
}

static void json_str(const std::string &s) {
  std::putchar('"');
  for (char c : s) {
    switch (c) {
    case '"':
      std::fputs("\\\"", stdout);
      break;
    case '\\':
      std::fputs("\\\\", stdout);
      break;
    case '\n':
      std::fputs("\\n", stdout);
      break;
    case '\r':
      std::fputs("\\r", stdout);
      break;
    case '\t':
      std::fputs("\\t", stdout);
      break;
    default:
      std::putchar(c);
    }
  }
  std::putchar('"');
}

void report_json(const VarList &vl, const std::string &filepath) {
  int has_values = 0;
  for (const auto &var : vl) {
    if (!var.value.empty()) {
      has_values = 1;
      break;
    }
  }

  std::printf("{\n");
  std::printf("  \"file\": ");
  json_str(filepath);
  std::printf(",\n");
  std::printf("  \"mode\": \"%s\",\n", has_values ? "runtime" : "static");
  std::printf("  \"variable_count\": %zu,\n", vl.size());
  std::printf("  \"variables\": [\n");

  for (size_t i = 0; i < vl.size(); i++) {
    const auto &v = vl[i];
    std::printf("    {\n");
    std::printf("      \"name\": ");
    json_str(v.name);
    std::printf(",\n");
    std::printf("      \"type\": ");
    json_str(v.type);
    std::printf(",\n");
    std::printf("      \"scope\": ");
    json_str(v.scope_name);
    std::printf(",\n");
    std::printf("      \"scope_kind\": ");
    json_str(scope_str(v.scope_kind));
    std::printf(",\n");
    std::printf("      \"line\": %u,\n", v.line);
    std::printf("      \"col\": %u", v.col);
    if (has_values) {
      std::printf(",\n");
      std::printf("      \"value\": ");
      json_str(v.value);
      std::printf("\n");
    } else {
      std::printf("\n");
    }
    std::printf("    }%s\n", i + 1 < vl.size() ? "," : "");
  }

  std::printf("  ]\n}\n");
}
