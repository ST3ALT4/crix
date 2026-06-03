#ifndef REPORT_H
#define REPORT_H

#include "static_analyzer.h"
#include <string>

void report_table(const VarList &vl, const std::string &filepath);
void report_json(const VarList &vl, const std::string &filepath);

#endif
