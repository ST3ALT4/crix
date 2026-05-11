#ifndef REPORT_H
#define REPORT_H

#include "static_analyzer.h"

void report_table(const VarList *vl, const char *filepath);
void report_json(const VarList *vl, const char *filepath);

#endif
