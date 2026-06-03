#ifndef AST_VISITOR_H
#define AST_VISITOR_H

#include "static_analyzer.h"
#include <clang-c/Index.h>

/* Walk the TU cursor tree and populate vl */
void ast_visit(CXTranslationUnit tu, VarList &vl);

#endif
