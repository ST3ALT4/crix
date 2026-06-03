#include <string>
#include <vector>
#include <cstdio>

#include <clang-c/Index.h>

#include "ast_visitor.h"
#include "static_analyzer.h"

struct ScopeFrame {
  std::string name;
  ScopeKind kind;
};

struct VisitCtx {
  VarList &vl;
  std::vector<ScopeFrame> stack;
};

static std::string cx_to_std_string(CXString s) {
  const char *p = clang_getCString(s);
  std::string result = p ? p : "";
  clang_disposeString(s);
  return result;
}

static void scope_push(VisitCtx &ctx, const std::string &name, ScopeKind kind) {
  ctx.stack.push_back({name, kind});
}

static void scope_pop(VisitCtx &ctx) {
  if (ctx.stack.size() > 1) { // Keep global scope seeded at index 0
    ctx.stack.pop_back();
  }
}

static ScopeFrame &current_scope(VisitCtx &ctx) {
  return ctx.stack.back();
}

static enum CXChildVisitResult visitor(CXCursor cursor, CXCursor parent,
                                       CXClientData data) {
  (void)parent;
  VisitCtx *ctx = static_cast<VisitCtx *>(data);

  /* Only look at cursors from the main file */
  CXSourceLocation loc = clang_getCursorLocation(cursor);
  if (clang_Location_isInSystemHeader(loc))
    return CXChildVisit_Continue;

  enum CXCursorKind kind = clang_getCursorKind(cursor);

  if (kind == CXCursor_FunctionDecl || kind == CXCursor_CXXMethod ||
      kind == CXCursor_Constructor || kind == CXCursor_Destructor) {
    std::string fname = cx_to_std_string(clang_getCursorSpelling(cursor));
    scope_push(*ctx, fname, ScopeKind::FUNCTION);
    clang_visitChildren(cursor, visitor, ctx);
    scope_pop(*ctx);
    return CXChildVisit_Continue;
  }

  if (kind == CXCursor_CompoundStmt) {
    /* anonymous block — inherit parent scope name but mark as block */
    ScopeFrame &cur = current_scope(*ctx);
    char bname[512];
    std::snprintf(bname, sizeof bname, "%s{}", cur.name.c_str());
    scope_push(*ctx, bname, ScopeKind::BLOCK);
    clang_visitChildren(cursor, visitor, ctx);
    scope_pop(*ctx);
    return CXChildVisit_Continue;
  }

  if (kind == CXCursor_StructDecl || kind == CXCursor_UnionDecl) {
    std::string sname = cx_to_std_string(clang_getCursorSpelling(cursor));
    if (sname.empty())
      sname = "<anon>";
    scope_push(*ctx, sname, ScopeKind::STRUCT);
    clang_visitChildren(cursor, visitor, ctx);
    scope_pop(*ctx);
    return CXChildVisit_Continue;
  }

  if (kind == CXCursor_EnumDecl) {
    std::string ename = cx_to_std_string(clang_getCursorSpelling(cursor));
    if (ename.empty())
      ename = "<anon>";
    scope_push(*ctx, ename, ScopeKind::ENUM);
    clang_visitChildren(cursor, visitor, ctx);
    scope_pop(*ctx);
    return CXChildVisit_Continue;
  }

  if (kind == CXCursor_VarDecl || kind == CXCursor_ParmDecl ||
      kind == CXCursor_FieldDecl || kind == CXCursor_EnumConstantDecl) {
    VarInfo v;

    v.name = cx_to_std_string(clang_getCursorSpelling(cursor));

    CXType ctype = clang_getCursorType(cursor);
    v.type = cx_to_std_string(clang_getTypeSpelling(ctype));

    ScopeFrame &sf = current_scope(*ctx);
    v.scope_name = sf.name;
    v.scope_kind = sf.kind;

    unsigned int line = 0, col = 0;
    clang_getSpellingLocation(loc, nullptr, &line, &col, nullptr);
    v.line = line;
    v.col = col;

    ctx->vl.push_back(v);

    clang_visitChildren(cursor, visitor, ctx);
    return CXChildVisit_Continue;
  }

  return CXChildVisit_Recurse;
}

void ast_visit(CXTranslationUnit tu, VarList &vl) {
  VisitCtx ctx{vl, {}};
  /* Seed stack with global scope */
  ctx.stack.push_back({"<global>", ScopeKind::GLOBAL});

  CXCursor root = clang_getTranslationUnitCursor(tu);
  clang_visitChildren(root, visitor, &ctx);
}
