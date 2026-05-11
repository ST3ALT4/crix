#include <stdio.h>
#include <string.h>

#include <clang-c/Index.h>

#include "ast_visitor.h"
#include "static_analyzer.h"

#define SCOPE_STACK_MAX 64

typedef struct {
  char name[256];
  ScopeKind kind;
} ScopeFrame;

typedef struct {
  VarList *vl;
  ScopeFrame stack[SCOPE_STACK_MAX];
  int depth;
} VisitCtx;

static const char *scope_kind_str(ScopeKind k) {
  switch (k) {
  case SCOPE_GLOBAL:
    return "global";
  case SCOPE_FUNCTION:
    return "function";
  case SCOPE_BLOCK:
    return "block";
  case SCOPE_STRUCT:
    return "struct";
  case SCOPE_ENUM:
    return "enum";
  }
  return "unknown";
}

static void cx_str_copy(char *dst, size_t dstsz, CXString s) {
  const char *p = clang_getCString(s);
  if (p)
    strncpy(dst, p, dstsz - 1);
  else
    dst[0] = '\0';
  dst[dstsz - 1] = '\0';
  clang_disposeString(s);
}

static void scope_push(VisitCtx *ctx, const char *name, ScopeKind kind) {
  if (ctx->depth >= SCOPE_STACK_MAX - 1)
    return;
  ctx->depth++;
  strncpy(ctx->stack[ctx->depth].name, name,
          sizeof ctx->stack[ctx->depth].name - 1);
  ctx->stack[ctx->depth].kind = kind;
}

static void scope_pop(VisitCtx *ctx) {
  if (ctx->depth > 0)
    ctx->depth--;
}

static ScopeFrame *current_scope(VisitCtx *ctx) {
  return &ctx->stack[ctx->depth];
}

static enum CXChildVisitResult visitor(CXCursor cursor, CXCursor parent,
                                       CXClientData data) {
  (void)parent;
  VisitCtx *ctx = (VisitCtx *)data;

  /* Only look at cursors from the main file */
  CXSourceLocation loc = clang_getCursorLocation(cursor);
  if (clang_Location_isInSystemHeader(loc))
    return CXChildVisit_Continue;

  enum CXCursorKind kind = clang_getCursorKind(cursor);

  if (kind == CXCursor_FunctionDecl || kind == CXCursor_CXXMethod ||
      kind == CXCursor_Constructor || kind == CXCursor_Destructor) {
    char fname[256];
    cx_str_copy(fname, sizeof fname, clang_getCursorSpelling(cursor));
    scope_push(ctx, fname, SCOPE_FUNCTION);
    clang_visitChildren(cursor, visitor, ctx);
    scope_pop(ctx);
    return CXChildVisit_Continue;
  }

  if (kind == CXCursor_CompoundStmt) {
    /* anonymous block — inherit parent scope name but mark as block */
    ScopeFrame *cur = current_scope(ctx);
    char bname[256];
    snprintf(bname, sizeof bname, "%s{}", cur->name);
    scope_push(ctx, bname, SCOPE_BLOCK);
    clang_visitChildren(cursor, visitor, ctx);
    scope_pop(ctx);
    return CXChildVisit_Continue;
  }

  if (kind == CXCursor_StructDecl || kind == CXCursor_UnionDecl) {
    char sname[256];
    cx_str_copy(sname, sizeof sname, clang_getCursorSpelling(cursor));
    if (sname[0] == '\0')
      snprintf(sname, sizeof sname, "<anon>");
    scope_push(ctx, sname, SCOPE_STRUCT);
    clang_visitChildren(cursor, visitor, ctx);
    scope_pop(ctx);
    return CXChildVisit_Continue;
  }

  if (kind == CXCursor_EnumDecl) {
    char ename[256];
    cx_str_copy(ename, sizeof ename, clang_getCursorSpelling(cursor));
    if (ename[0] == '\0')
      snprintf(ename, sizeof ename, "<anon>");
    scope_push(ctx, ename, SCOPE_ENUM);
    clang_visitChildren(cursor, visitor, ctx);
    scope_pop(ctx);
    return CXChildVisit_Continue;
  }

  if (kind == CXCursor_VarDecl || kind == CXCursor_ParmDecl ||
      kind == CXCursor_FieldDecl || kind == CXCursor_EnumConstantDecl) {
    VarInfo v = {0};

    cx_str_copy(v.name, sizeof v.name, clang_getCursorSpelling(cursor));

    CXType ctype = clang_getCursorType(cursor);
    cx_str_copy(v.type, sizeof v.type, clang_getTypeSpelling(ctype));

    ScopeFrame *sf = current_scope(ctx);
    strncpy(v.scope_name, sf->name, sizeof v.scope_name - 1);
    v.scope_kind = sf->kind;

    unsigned line = 0, col = 0;
    clang_getSpellingLocation(loc, NULL, &line, &col, NULL);
    v.line = line;
    v.col = col;

    varlist_push(ctx->vl, &v);

    clang_visitChildren(cursor, visitor, ctx);
    return CXChildVisit_Continue;
  }

  return CXChildVisit_Recurse;
}

void ast_visit(CXTranslationUnit tu, VarList *vl) {
  VisitCtx ctx = {0};
  ctx.vl = vl;
  /* Seed stack with global scope */
  strncpy(ctx.stack[0].name, "<global>", sizeof ctx.stack[0].name - 1);
  ctx.stack[0].kind = SCOPE_GLOBAL;
  ctx.depth = 0;

  CXCursor root = clang_getTranslationUnitCursor(tu);
  clang_visitChildren(root, visitor, &ctx);
}
