#include "trace_model.h"
#include <sstream>
#include <algorithm>
#include <iomanip>

static std::string escape_json(const std::string &s) {
  std::ostringstream o;
  for (char c : s) {
    switch (c) {
    case '"': o << "\\\""; break;
    case '\\': o << "\\\\"; break;
    case '\b': o << "\\b"; break;
    case '\f': o << "\\f"; break;
    case '\n': o << "\\n"; break;
    case '\r': o << "\\r"; break;
    case '\t': o << "\\t"; break;
    default:
      if ('\x00' <= c && c <= '\x1f') {
        o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
      } else {
        o << c;
      }
    }
  }
  return o.str();
}

std::string RuntimeValue::to_string() const {
  if (kind == ValueKind::ARRAY) {
    // Summarize char arrays as strings instead of element-by-element
    if (type_name.find("char") != std::string::npos && !elements.empty()) {
      std::string str;
      for (const auto &e : elements) {
        const std::string &rv = e.raw_value;
        // DAP returns char values as 'x' or '\0' etc.
        if (rv.size() >= 3 && rv.front() == '\'' && rv.back() == '\'') {
          std::string inner = rv.substr(1, rv.size() - 2);
          if (inner == "\\0" || inner == "\\x00") break; // null terminator
          if (inner == "\\n") { str += '\n'; continue; }
          if (inner == "\\t") { str += '\t'; continue; }
          if (inner == "\\r") { str += '\r'; continue; }
          if (inner == "\\'") { str += '\''; continue; }
          if (inner == "\\\\") { str += '\\'; continue; }
          str += inner;
        } else if (rv == "0" || rv == "\\0") {
          break; // null terminator as integer
        } else {
          str += rv;
        }
      }
      return "\"" + str + "\"";
    }

    std::string res = "[";
    for (size_t i = 0; i < elements.size(); ++i) {
      if (i > 0) res += ", ";
      res += elements[i].to_string();
    }
    res += "]";
    return res;
  }
  if (kind == ValueKind::STRUCT) {
    std::string res = "{";
    for (size_t i = 0; i < fields.size(); ++i) {
      if (i > 0) res += ", ";
      res += fields[i].first + ": " + fields[i].second.to_string();
    }
    res += "}";
    return res;
  }
  return raw_value.empty() ? "?" : raw_value;
}

static void value_to_json(const RuntimeValue &val, std::ostringstream &ss, int indent) {
  std::string ind(indent, ' ');
  ss << "{\n";
  ss << ind << "  \"type\": \"" << escape_json(val.type_name) << "\",\n";
  ss << ind << "  \"kind\": \"";
  switch (val.kind) {
  case ValueKind::PRIMITIVE: ss << "primitive"; break;
  case ValueKind::ARRAY: ss << "array"; break;
  case ValueKind::STRUCT: ss << "struct"; break;
  case ValueKind::POINTER: ss << "pointer"; break;
  default: ss << "other"; break;
  }
  ss << "\",\n";
  ss << ind << "  \"value\": \"" << escape_json(val.raw_value) << "\"";

  if (val.kind == ValueKind::ARRAY) {
    ss << ",\n" << ind << "  \"length\": " << val.elements.size() << ",\n";
    ss << ind << "  \"elements\": [";
    for (size_t i = 0; i < val.elements.size(); ++i) {
      if (i > 0) ss << ", ";
      ss << "\"" << escape_json(val.elements[i].to_string()) << "\"";
    }
    ss << "]";
  } else if (val.kind == ValueKind::STRUCT) {
    ss << ",\n" << ind << "  \"fields\": {\n";
    for (size_t i = 0; i < val.fields.size(); ++i) {
      ss << ind << "    \"" << escape_json(val.fields[i].first) << "\": ";
      value_to_json(val.fields[i].second, ss, indent + 4);
      if (i + 1 < val.fields.size()) ss << ",";
      ss << "\n";
    }
    ss << ind << "  }";
  }
  ss << "\n" << ind << "}";
}

StepDelta compute_delta(const StepRecord &prev, const StepRecord &curr) {
  StepDelta delta;
  std::unordered_map<std::string, const RuntimeValue*> prev_map;
  for (const auto &p : prev.variables) {
    prev_map[p.first] = &p.second;
  }

  for (const auto &c : curr.variables) {
    auto it = prev_map.find(c.first);
    if (it == prev_map.end()) {
      // Newly created/initialized variable
      delta.modified_vars.push_back(c.first);
    } else {
      const RuntimeValue *pv = it->second;
      const RuntimeValue &cv = c.second;
      if (pv->to_string() != cv.to_string()) {
        delta.modified_vars.push_back(c.first);
        if (cv.kind == ValueKind::ARRAY && pv->kind == ValueKind::ARRAY) {
          size_t count = std::max(pv->elements.size(), cv.elements.size());
          for (size_t idx = 0; idx < count; ++idx) {
            std::string p_elem = (idx < pv->elements.size()) ? pv->elements[idx].to_string() : "<none>";
            std::string c_elem = (idx < cv.elements.size()) ? cv.elements[idx].to_string() : "<none>";
            if (p_elem != c_elem) {
              delta.array_mutations.push_back({c.first, idx, p_elem, c_elem});
            }
          }
        }
      }
    }
  }
  return delta;
}

std::string ExecutionTrace::to_json(bool pretty) const {
  (void)pretty;
  std::ostringstream ss;
  ss << "{\n";
  ss << "  \"source_file\": \"" << escape_json(source_file) << "\",\n";
  ss << "  \"total_steps\": " << steps.size() << ",\n";
  ss << "  \"steps\": [\n";

  for (size_t i = 0; i < steps.size(); ++i) {
    const auto &s = steps[i];
    ss << "    {\n";
    ss << "      \"step\": " << s.step_number << ",\n";
    ss << "      \"file\": \"" << escape_json(s.file) << "\",\n";
    ss << "      \"line\": " << s.line << ",\n";
    ss << "      \"column\": " << s.col << ",\n";
    ss << "      \"function\": \"" << escape_json(s.function_name) << "\",\n";

    // Call stack
    ss << "      \"call_stack\": [";
    for (size_t f = 0; f < s.call_stack.size(); ++f) {
      if (f > 0) ss << ", ";
      ss << "\"" << escape_json(s.call_stack[f].name) << "\"";
    }
    ss << "],\n";

    // Variables
    ss << "      \"variables\": {\n";
    for (size_t v = 0; v < s.variables.size(); ++v) {
      ss << "        \"" << escape_json(s.variables[v].first) << "\": ";
      value_to_json(s.variables[v].second, ss, 8);
      if (v + 1 < s.variables.size()) ss << ",";
      ss << "\n";
    }
    ss << "      },\n";

    // Deltas
    ss << "      \"delta\": {\n";
    ss << "        \"modified_vars\": [";
    for (size_t m = 0; m < s.delta.modified_vars.size(); ++m) {
      if (m > 0) ss << ", ";
      ss << "\"" << escape_json(s.delta.modified_vars[m]) << "\"";
    }
    ss << "],\n";
    ss << "        \"array_mutations\": [\n";
    for (size_t a = 0; a < s.delta.array_mutations.size(); ++a) {
      const auto &mut = s.delta.array_mutations[a];
      ss << "          {\"variable\": \"" << escape_json(mut.variable_name) << "\", ";
      ss << "\"index\": " << mut.index << ", ";
      ss << "\"prev\": \"" << escape_json(mut.prev_value) << "\", ";
      ss << "\"curr\": \"" << escape_json(mut.curr_value) << "\"}";
      if (a + 1 < s.delta.array_mutations.size()) ss << ",";
      ss << "\n";
    }
    ss << "        ]\n";
    ss << "      },\n";
    ss << "      \"stdout\": \"" << escape_json(s.stdout_chunk) << "\"\n";
    ss << "    }" << (i + 1 < steps.size() ? "," : "") << "\n";
  }

  ss << "  ]\n";
  ss << "}\n";
  return ss.str();
}
