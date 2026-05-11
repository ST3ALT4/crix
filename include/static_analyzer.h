#ifndef STATIC_ANALYZER_H
#define STATIC_ANALYZER_H

#include <stddef.h>

typedef enum { FMT_TABLE, FMT_JSON } OutputFormat;

typedef enum {
  SCOPE_GLOBAL,
  SCOPE_FUNCTION,
  SCOPE_BLOCK,
  SCOPE_STRUCT,
  SCOPE_ENUM
} ScopeKind;

typedef struct {
  char name[256];
  char type[256];
  char scope_name[256];
  ScopeKind scope_kind;
  unsigned line;
  unsigned col;
  char value[256];
} VarInfo;

typedef struct {
  VarInfo *items;
  size_t count;
  size_t capacity;
} VarList;

VarList *varlist_new(void);
void varlist_push(VarList *vl, const VarInfo *v);
void varlist_free(VarList *vl);

VarList *static_analyze(const char *filepath, const char **extra_args,
                        int extra_argc);

#endif
