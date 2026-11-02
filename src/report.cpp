#include "report.h"
#include "trace_model.h"
#include "lsp_analyzer.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_set>
#include <cstdio>

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

static const char* lsp_kind_str(int kind) {
  switch (kind) {
  case static_cast<int>(LspSymbolKind::Variable):   return "variable";
  case static_cast<int>(LspSymbolKind::Field):       return "field";
  case static_cast<int>(LspSymbolKind::Constant):    return "constant";
  case static_cast<int>(LspSymbolKind::EnumMember):  return "enum-member";
  case static_cast<int>(LspSymbolKind::Property):    return "property";
  default:                                            return "var";
  }
}

static void json_str(const std::string& s) {
  std::putchar('"');
  for (char c : s) {
    switch (c) {
    case '"':  std::fputs("\\\"", stdout); break;
    case '\\': std::fputs("\\\\", stdout); break;
    case '\n': std::fputs("\\n",  stdout); break;
    case '\r': std::fputs("\\r",  stdout); break;
    case '\t': std::fputs("\\t",  stdout); break;
    default:   std::putchar(c);
    }
  }
  std::putchar('"');
}

// ─────────────────────────────────────────────────────────────────────────────
// Output A: Step-by-step timeline
// ─────────────────────────────────────────────────────────────────────────────

void report_timeline(const ExecutionTrace& trace, bool interactive) {
  std::cout << "\n======================================================================\n";
  std::cout << "  CRIX Dynamic Execution Timeline (DSA Animation Data)\n";
  std::cout << "  File: " << trace.source_file
            << " | Steps Recorded: " << trace.steps.size() << "\n";
  std::cout << "======================================================================\n";

  for (size_t idx = 0; idx < trace.steps.size(); ++idx) {
    const auto& s = trace.steps[idx];

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

    std::unordered_set<std::string> mod_set(
        s.delta.modified_vars.begin(), s.delta.modified_vars.end());
    std::unordered_set<size_t> mutated_indices;
    for (const auto& mut : s.delta.array_mutations) {
      mutated_indices.insert(mut.index);
    }

    for (const auto& p : s.variables) {
      bool is_mod = mod_set.count(p.first) > 0;

      // Scope annotation from LSP (if available)
      std::string scope_tag;
      auto sc_it = s.var_scope_names.find(p.first);
      if (sc_it != s.var_scope_names.end() && !sc_it->second.empty()
          && sc_it->second != "<global>") {
        scope_tag = " \033[90m[" + sc_it->second + "]\033[0m";
      }

      std::cout << "  " << (is_mod ? "\033[1;32m* " : "  ") << p.first
                << scope_tag
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
      std::cout << "  \033[90m-- (Press Enter to continue, 'q' to end)... \033[0m"
                << std::flush;
      std::string input;
      std::getline(std::cin, input);
      if (input == "q" || input == "Q") interactive = false;
    }
  }
  std::cout << "\n======================================================================\n\n";
}

bool export_trace_json(const ExecutionTrace& trace, const std::string& output_path) {
  std::ofstream out(output_path);
  if (!out.is_open()) {
    std::cerr << "Failed to open " << output_path << " for writing trace JSON.\n";
    return false;
  }
  out << trace.to_json(true);
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Output B: Final-state variable summary
// ─────────────────────────────────────────────────────────────────────────────

// Extract the final variable snapshot: walk steps in reverse to find the last
// recorded value for each unique variable name.
static std::vector<std::pair<std::string, RuntimeValue>>
final_variable_snapshot(const ExecutionTrace& trace) {
  std::vector<std::pair<std::string, RuntimeValue>> snap;
  std::unordered_set<std::string> seen;

  for (int i = static_cast<int>(trace.steps.size()) - 1; i >= 0; --i) {
    for (const auto& p : trace.steps[i].variables) {
      if (seen.insert(p.first).second) {
        snap.push_back(p);
      }
    }
  }
  return snap;
}

void report_summary_table(const ExecutionTrace& trace, const std::string& filepath) {
  if (trace.steps.empty()) {
    std::printf("\n[crix] No execution steps recorded for %s\n", filepath.c_str());
    return;
  }

  const auto  snap  = final_variable_snapshot(trace);
  const auto& last  = trace.steps.back();
  const bool  has_scope = !last.var_scope_names.empty();

  // Column widths
  int w_name = 12, w_type = 20, w_scope = 16, w_val = 16;
  for (const auto& p : snap) {
    int n;
    if ((n = (int)p.first.size())              > w_name)  w_name  = n;
    if ((n = (int)p.second.type_name.size())   > w_type)  w_type  = n;
    if ((n = (int)p.second.to_string().size()) > w_val)   w_val   = n;
    if (has_scope) {
      auto it = last.var_scope_names.find(p.first);
      if (it != last.var_scope_names.end() && (n = (int)it->second.size()) > w_scope)
        w_scope = n;
    }
  }

  std::printf("\n======================================================================\n");
  std::printf("  CRIX — Runtime Variable Summary\n");
  std::printf("  File: %s | Variables: %zu\n", filepath.c_str(), snap.size());
  std::printf("======================================================================\n\n");

  if (has_scope) {
    std::printf("%-*s  %-*s  %-*s  %-*s\n",
        w_name, "NAME", w_type, "TYPE", w_scope, "SCOPE", w_val, "VALUE");
  } else {
    std::printf("%-*s  %-*s  %-*s\n",
        w_name, "NAME", w_type, "TYPE", w_val, "VALUE");
  }

  int sep = w_name + w_type + w_val + 4 + (has_scope ? w_scope + 2 : 0);
  for (int i = 0; i < sep; ++i) std::putchar('-');
  std::putchar('\n');

  for (const auto& p : snap) {
    std::string scope;
    if (has_scope) {
      auto it = last.var_scope_names.find(p.first);
      if (it != last.var_scope_names.end()) scope = it->second;
    }
    if (has_scope) {
      std::printf("%-*s  %-*s  %-*s  %-*s\n",
          w_name, p.first.c_str(),
          w_type, p.second.type_name.c_str(),
          w_scope, scope.c_str(),
          w_val,  p.second.to_string().c_str());
    } else {
      std::printf("%-*s  %-*s  %-*s\n",
          w_name, p.first.c_str(),
          w_type, p.second.type_name.c_str(),
          w_val,  p.second.to_string().c_str());
    }
  }
  std::putchar('\n');
}

void report_summary_json(const ExecutionTrace& trace, const std::string& filepath) {
  if (trace.steps.empty()) {
    std::printf("{\"file\":");
    json_str(filepath);
    std::printf(",\"variables\":[]}\n");
    return;
  }

  const auto  snap = final_variable_snapshot(trace);
  const auto& last = trace.steps.back();

  std::printf("{\n  \"file\": ");
  json_str(filepath);
  std::printf(",\n  \"mode\": \"trace-summary\",\n");
  std::printf("  \"total_steps\": %zu,\n", trace.steps.size());
  std::printf("  \"variable_count\": %zu,\n", snap.size());
  std::printf("  \"variables\": [\n");

  for (size_t i = 0; i < snap.size(); ++i) {
    const auto& [name, val] = snap[i];
    std::printf("    {\n");
    std::printf("      \"name\": "); json_str(name); std::printf(",\n");
    std::printf("      \"type\": "); json_str(val.type_name); std::printf(",\n");
    std::printf("      \"value\": "); json_str(val.to_string());

    // LSP annotations if present
    auto sc_it = last.var_scope_names.find(name);
    if (sc_it != last.var_scope_names.end()) {
      std::printf(",\n      \"scope\": "); json_str(sc_it->second);
    }
    auto ki_it = last.var_symbol_kinds.find(name);
    if (ki_it != last.var_symbol_kinds.end()) {
      std::printf(",\n      \"lsp_kind\": \"%s\"", lsp_kind_str(ki_it->second));
    }

    std::printf("\n    }%s\n", i + 1 < snap.size() ? "," : "");
  }
  std::printf("  ]\n}\n");
}
