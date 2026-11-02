#ifndef REPORT_H
#define REPORT_H

#include "trace_model.h"
#include <string>

// Output A: Step-by-step execution timeline for DSA visualizer
void report_timeline(const ExecutionTrace& trace, bool interactive = false);
bool export_trace_json(const ExecutionTrace& trace, const std::string& output_path);

// Output B: Final-state variable summary (derived from the last trace step)
// Only produced when the user passes --summary.
void report_summary_table(const ExecutionTrace& trace, const std::string& filepath);
void report_summary_json(const ExecutionTrace& trace, const std::string& filepath);

#endif // REPORT_H
