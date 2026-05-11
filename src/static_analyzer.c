#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <clang-c/Index.h>

#include "ast_visitor.h"
#include "static_analyzer.h"

VarList *varlist_new(void) {
  VarList *vl = calloc(1, sizeof *vl);
  if (!vl) {
    perror("calloc");
    exit(EXIT_FAILURE);
  }
  vl->capacity = 64;
  vl->items = malloc(vl->capacity * sizeof(VarInfo));
  if (!vl->items) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  return vl;
}

void varlist_push(VarList *vl, const VarInfo *v) {
  if (vl->count == vl->capacity) {
    vl->capacity *= 2;
    vl->items = realloc(vl->items, vl->capacity * sizeof(VarInfo));
    if (!vl->items) {
      perror("realloc");
      exit(EXIT_FAILURE);
    }
  }
  vl->items[vl->count++] = *v;
}

void varlist_free(VarList *vl) {
  if (!vl)
    return;
  free(vl->items);
  free(vl);
}

VarList *static_analyze(const char *filepath, const char **extra_args,
                        int extra_argc) {
  CXIndex idx = clang_createIndex(0, 0);
  if (!idx) {
    fputs("clang_createIndex failed\n", stderr);
    return NULL;
  }

  /* Parse the file */
  CXTranslationUnit tu = clang_parseTranslationUnit(
      idx, filepath, extra_args, extra_argc, NULL, 0,
      CXTranslationUnit_DetailedPreprocessingRecord |
          CXTranslationUnit_SkipFunctionBodies /* keep for speed; remove to
                                                  resolve types inside bodies */
              ^ CXTranslationUnit_SkipFunctionBodies /* actually walk bodies for
                                                        locals */
  );

  if (!tu) {
    fprintf(stderr, "Failed to parse: %s\n", filepath);
    clang_disposeIndex(idx);
    return NULL;
  }

  /* Report diagnostics */
  unsigned num_diag = clang_getNumDiagnostics(tu);
  for (unsigned i = 0; i < num_diag; i++) {
    CXDiagnostic d = clang_getDiagnostic(tu, i);
    CXString ds = clang_formatDiagnostic(d, CXDiagnostic_DisplaySourceLocation |
                                                CXDiagnostic_DisplayOption);
    fprintf(stderr, "diag: %s\n", clang_getCString(ds));
    clang_disposeString(ds);
    clang_disposeDiagnostic(d);
  }

  VarList *vl = varlist_new();
  ast_visit(tu, vl);

  clang_disposeTranslationUnit(tu);
  clang_disposeIndex(idx);
  return vl;
}
