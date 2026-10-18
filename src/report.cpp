#include "report.h"
#include "static_analyzer.h"
#include "trace_model.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <unordered_set>

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
  int has_values = 0;
  for (const auto &var : vl) {
    if (!var.value.empty()) {
      has_values = 1;
      break;
    }
  }

  const char *mode_label = has_values ? "Runtime" : "Static";
  std::printf("\n======================================================================\n");
  std::printf("  CRIX (CXX Resource Input Explorer) — %s Analysis\n", mode_label);
  std::printf("  File: %s | Total Variables: %zu\n", filepath.c_str(), vl.size());
  std::printf("======================================================================\n\n");

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

void report_timeline(const ExecutionTrace &trace, bool interactive) {
  std::cout << "\n======================================================================\n";
  std::cout << "  CRIX Dynamic Execution Timeline (DSA Animation Data)\n";
  std::cout << "  File: " << trace.source_file << " | Steps Recorded: " << trace.steps.size() << "\n";
  std::cout << "======================================================================\n";

  for (size_t idx = 0; idx < trace.steps.size(); ++idx) {
    const auto &s = trace.steps[idx];

    std::cout << "\n\033[1;36m[Step " << s.step_number << "]\033[0m "
              << "\033[1m" << s.file << ":" << s.line << "\033[0m in "
              << "\033[1;33m" << s.function_name << "()\033[0m";

    if (s.call_stack.size() > 1) {
      std::cout << "  \033[90m(stack:";
      for (size_t f = 0; f < s.call_stack.size(); ++f) {
        std::cout << (f == 0 ? " " : " -> ") << s.call_stack[f].name;
      }
      std::cout << ")\033[0m";
    }
    std::cout << "\n";

    std::unordered_set<std::string> mod_set(s.delta.modified_vars.begin(), s.delta.modified_vars.end());
    std::unordered_set<size_t> mutated_indices;
    for (const auto &mut : s.delta.array_mutations) {
      mutated_indices.insert(mut.index);
    }

    for (const auto &p : s.variables) {
      bool is_mod = (mod_set.find(p.first) != mod_set.end());
      std::cout << "  " << (is_mod ? "\033[1;32m* " : "  ") << p.first
                << " \033[90m(" << p.second.type_name << ")\033[0m = ";

      if (p.second.kind == ValueKind::ARRAY) {
        std::cout << "[";
        for (size_t el_i = 0; el_i < p.second.elements.size(); ++el_i) {
          if (el_i > 0) std::cout << ", ";
          if (mutated_indices.count(el_i)) {
            std::cout << "\033[1;31m" << p.second.elements[el_i].to_string() << "\033[0m";
          } else {
            std::cout << p.second.elements[el_i].to_string();
          }
        }
        std::cout << "]";
      } else {
        if (is_mod) {
          std::cout << "\033[1;32m" << p.second.to_string() << "\033[0m";
        } else {
          std::cout << p.second.to_string();
        }
      }
      if (is_mod) std::cout << "\033[0m";
      std::cout << "\n";
    }

    if (!s.stdout_chunk.empty()) {
      std::cout << "  \033[35m[stdout]\033[0m " << s.stdout_chunk;
      if (s.stdout_chunk.back() != '\n') std::cout << "\n";
    }

    if (interactive && idx + 1 < trace.steps.size()) {
      std::cout << "  \033[90m-- (Press Enter to continue, 'q' to end interactive)... \033[0m" << std::flush;
      std::string input;
      std::getline(std::cin, input);
      if (input == "q" || input == "Q") {
        interactive = false;
      }
    }
  }
  std::cout << "\n======================================================================\n\n";
}

bool export_trace_json(const ExecutionTrace &trace, const std::string &output_path) {
  std::ofstream out(output_path);
  if (!out.is_open()) {
    std::cerr << "Failed to open " << output_path << " for writing trace JSON.\n";
    return false;
  }
  out << trace.to_json(true);
  return true;
}
