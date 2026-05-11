#include <stdio.h>
#include <string.h>

#include "report.h"
#include "static_analyzer.h"

static const char *scope_str(ScopeKind k) {
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
  return "?";
}

void report_table(const VarList *vl, const char *filepath) {
  /* Detect whether any entry has a non-empty value (runtime mode) */
  int has_values = 0;
  for (size_t i = 0; i < vl->count; i++)
    if (vl->items[i].value[0] != '\0') {
      has_values = 1;
      break;
    }

  const char *mode_label = has_values ? "Runtime" : "Static";
  printf("\nVarScope — %s Analysis: %s\n", mode_label, filepath);
  printf("Variables found: %zu\n\n", vl->count);

  /* column widths */
  int w_name = 12, w_type = 20, w_scope = 16, w_skind = 10, w_val = 14;

  for (size_t i = 0; i < vl->count; i++) {
    int n;
    if ((n = (int)strlen(vl->items[i].name)) > w_name)
      w_name = n;
    if ((n = (int)strlen(vl->items[i].type)) > w_type)
      w_type = n;
    if ((n = (int)strlen(vl->items[i].scope_name)) > w_scope)
      w_scope = n;
    if (has_values && (n = (int)strlen(vl->items[i].value)) > w_val)
      w_val = n;
  }

  /* header */
  if (has_values)
    printf("%-*s  %-*s  %-*s  %-*s  %5s  %4s  %-*s\n", w_name, "NAME", w_type,
           "TYPE", w_scope, "SCOPE", w_skind, "SCOPE-KIND", "LINE", "COL",
           w_val, "VALUE");
  else
    printf("%-*s  %-*s  %-*s  %-*s  %5s  %4s\n", w_name, "NAME", w_type, "TYPE",
           w_scope, "SCOPE", w_skind, "SCOPE-KIND", "LINE", "COL");

  int sep_len =
      w_name + w_type + w_scope + w_skind + 18 + (has_values ? w_val + 2 : 0);
  for (int i = 0; i < sep_len; i++)
    putchar('-');
  putchar('\n');

  for (size_t i = 0; i < vl->count; i++) {
    const VarInfo *v = &vl->items[i];
    if (has_values)
      printf("%-*s  %-*s  %-*s  %-*s  %5u  %4u  %-*s\n", w_name, v->name,
             w_type, v->type, w_scope, v->scope_name, w_skind,
             scope_str(v->scope_kind), v->line, v->col, w_val, v->value);
    else
      printf("%-*s  %-*s  %-*s  %-*s  %5u  %4u\n", w_name, v->name, w_type,
             v->type, w_scope, v->scope_name, w_skind, scope_str(v->scope_kind),
             v->line, v->col);
  }
  putchar('\n');
}

static void json_str(const char *s) {
  putchar('"');
  for (; *s; s++) {
    switch (*s) {
    case '"':
      fputs("\\\"", stdout);
      break;
    case '\\':
      fputs("\\\\", stdout);
      break;
    case '\n':
      fputs("\\n", stdout);
      break;
    case '\r':
      fputs("\\r", stdout);
      break;
    case '\t':
      fputs("\\t", stdout);
      break;
    default:
      putchar(*s);
    }
  }
  putchar('"');
}

void report_json(const VarList *vl, const char *filepath) {
  int has_values = 0;
  for (size_t i = 0; i < vl->count; i++)
    if (vl->items[i].value[0] != '\0') {
      has_values = 1;
      break;
    }

  printf("{\n");
  printf("  \"file\": ");
  json_str(filepath);
  printf(",\n");
  printf("  \"mode\": \"%s\",\n", has_values ? "runtime" : "static");
  printf("  \"variable_count\": %zu,\n", vl->count);
  printf("  \"variables\": [\n");

  for (size_t i = 0; i < vl->count; i++) {
    const VarInfo *v = &vl->items[i];
    printf("    {\n");
    printf("      \"name\": ");
    json_str(v->name);
    printf(",\n");
    printf("      \"type\": ");
    json_str(v->type);
    printf(",\n");
    printf("      \"scope\": ");
    json_str(v->scope_name);
    printf(",\n");
    printf("      \"scope_kind\": ");
    json_str(scope_str(v->scope_kind));
    printf(",\n");
    printf("      \"line\": %u,\n", v->line);
    printf("      \"col\": %u", v->col);
    if (has_values) {
      printf(",\n");
      printf("      \"value\": ");
      json_str(v->value);
      printf("\n");
    } else {
      printf("\n");
    }
    printf("    }%s\n", i + 1 < vl->count ? "," : "");
  }

  printf("  ]\n}\n");
}
