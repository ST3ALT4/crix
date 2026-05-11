#ifndef RUNTIME_ANALYZER_H
#define RUNTIME_ANALYZER_H

#include "static_analyzer.h"

VarList *runtime_analyze(const char *binary_path, char *const argv[]);

#endif
