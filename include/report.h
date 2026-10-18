#ifndef REPORT_H
#define REPORT_H

#include "static_analyzer.h"
#include "trace_model.h"
#include <string>

// Output B: Summary variable tables
void report_table(const VarList &vl, const std::string &filepath);
void report_json(const VarList &vl, const std::string &filepath);

// Output A: Step-by-step execution timeline for DSA visualizer
void report_timeline(const ExecutionTrace &trace, bool interactive = false);
bool export_trace_json(const ExecutionTrace &trace, const std::string &output_path);

#endif // REPORT_H
