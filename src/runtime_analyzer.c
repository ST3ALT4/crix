#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <dwarf.h>
#include <elfutils/libdw.h>

#include "runtime_analyzer.h"
#include "static_analyzer.h"

static uint64_t reg_val(const struct user_regs_struct *r, unsigned n) {
  switch (n) {
  case 0:
    return r->rax;
  case 1:
    return r->rdx;
  case 2:
    return r->rcx;
  case 3:
    return r->rbx;
  case 4:
    return r->rsi;
  case 5:
    return r->rdi;
  case 6:
    return r->rbp;
  case 7:
    return r->rsp;
  case 8:
    return r->r8;
  case 9:
    return r->r9;
  case 10:
    return r->r10;
  case 11:
    return r->r11;
  case 12:
    return r->r12;
  case 13:
    return r->r13;
  case 14:
    return r->r14;
  case 15:
    return r->r15;
  default:
    return 0;
  }
}

typedef enum { LOC_NONE, LOC_MEMORY, LOC_REGISTER } LocKind;

typedef struct {
  LocKind kind;
  uintptr_t addr; /* LOC_MEMORY:   address in tracee space  */
  uint64_t val;   /* LOC_REGISTER: direct integer value      */
} LocResult;

/* Evaluate a libdw Dwarf_Op[] expression.
 * load_offset: ASLR slide for PIE (0 for non-PIE).
 * NOTE: called after the function prologue has executed, so RBP is valid. */
static LocResult eval_ops(const Dwarf_Op *ops, size_t n,
                          const struct user_regs_struct *regs,
                          uintptr_t load_offset) {
  LocResult r = {LOC_NONE, 0, 0};
  if (!ops || n == 0)
    return r;

  uint64_t stk[64];
  int sp = 0;

  for (size_t i = 0; i < n; i++) {
    uint8_t atom = ops[i].atom;

    /* ---- register-value ops (result is the register itself) ---------- */
    if (atom >= DW_OP_reg0 && atom <= DW_OP_reg31) {
      r.kind = LOC_REGISTER;
      r.val = reg_val(regs, (unsigned)(atom - DW_OP_reg0));
      return r;
    }
    if (atom == DW_OP_regx) {
      r.kind = LOC_REGISTER;
      r.val = reg_val(regs, (unsigned)ops[i].number);
      return r;
    }
    if (atom == DW_OP_stack_value) {
      if (sp > 0) {
        r.kind = LOC_REGISTER;
        r.val = stk[sp - 1];
      }
      return r;
    }

    /* ---- breg: register + signed offset → address on stack ----------- */
    if (atom >= DW_OP_breg0 && atom <= DW_OP_breg31) {
      if (sp < 64)
        stk[sp++] =
            (uint64_t)((int64_t)reg_val(regs, (unsigned)(atom - DW_OP_breg0)) +
                       (int64_t)(Dwarf_Sword)ops[i].number);
      continue;
    }

    switch (atom) {
    /* absolute address (adjusted for PIE) */
    case DW_OP_addr:
      if (sp < 64)
        stk[sp++] = (uint64_t)ops[i].number + load_offset;
      break;

    /* CFA-relative (frame base = RBP on x86-64 -O0) */
    case DW_OP_fbreg:
      if (sp < 64)
        stk[sp++] = (uint64_t)((int64_t)regs->rbp +
                               (int64_t)(Dwarf_Sword)ops[i].number);
      break;

    case DW_OP_bregx:
      if (sp < 64)
        stk[sp++] = (uint64_t)((int64_t)reg_val(regs, (unsigned)ops[i].number) +
                               (int64_t)(Dwarf_Sword)ops[i].number2);
      break;

    case DW_OP_plus_uconst:
      if (sp > 0)
        stk[sp - 1] += ops[i].number;
      break;
    case DW_OP_plus:
      if (sp >= 2) {
        stk[sp - 2] += stk[sp - 1];
        sp--;
      }
      break;
    case DW_OP_minus:
      if (sp >= 2) {
        stk[sp - 2] -= stk[sp - 1];
        sp--;
      }
      break;
    case DW_OP_dup:
      if (sp > 0 && sp < 64) {
        stk[sp] = stk[sp - 1];
        sp++;
      }
      break;

    /* constants */
    case DW_OP_const1u:
    case DW_OP_const2u:
    case DW_OP_const4u:
    case DW_OP_const8u:
    case DW_OP_constu:
      if (sp < 64)
        stk[sp++] = ops[i].number;
      break;
    case DW_OP_const1s:
    case DW_OP_const2s:
    case DW_OP_const4s:
    case DW_OP_const8s:
    case DW_OP_consts:
      if (sp < 64)
        stk[sp++] = (uint64_t)(int64_t)(Dwarf_Sword)ops[i].number;
      break;

    default:
      if (atom >= 0x30 && atom <= 0x4f) {
        if (sp < 64)
          stk[sp++] = (uint64_t)(atom - 0x30u);
      }
      /* All other ops: skip (leave stack unchanged) */
      break;
    }
  }

  if (sp > 0) {
    r.kind = LOC_MEMORY;
    r.addr = (uintptr_t)stk[sp - 1];
  }
  return r;
}

static int peek_mem(pid_t pid, uintptr_t addr, void *buf, size_t len) {
  uint8_t *dst = buf;
  while (len) {
    errno = 0;
    long w = ptrace(PTRACE_PEEKDATA, pid, (void *)addr, NULL);
    if (errno)
      return -1;
    size_t take = len < (size_t)sizeof w ? len : (size_t)sizeof w;
    memcpy(dst, &w, take);
    dst += sizeof w;
    addr += (uintptr_t)sizeof w;
    len -= take;
  }
  return 0;
}

static void format_value(pid_t pid, const char *type, LocResult loc, char *out,
                         size_t sz) {
  if (loc.kind == LOC_NONE) {
    snprintf(out, sz, "<no-loc>");
    return;
  }

  int is_ptr = (strchr(type, '*') != NULL);
  int is_float = (strstr(type, "double") || strstr(type, "float"));
  int is_uns = (strstr(type, "unsigned") || strstr(type, "uint") ||
                strstr(type, "size_t") || strstr(type, "bool"));

  /* ---- register-held ----------------------------------------------- */
  if (loc.kind == LOC_REGISTER) {
    if (is_ptr && strstr(type, "char") && loc.val) {
      char s[48];
      if (peek_mem(pid, (uintptr_t)loc.val, s, sizeof s - 1) == 0) {
        s[sizeof s - 1] = '\0';
        snprintf(out, sz, "0x%llx (\"%.*s\")", (unsigned long long)loc.val, 40,
                 s);
      } else
        snprintf(out, sz, "0x%llx", (unsigned long long)loc.val);
    } else if (is_ptr)
      snprintf(out, sz, "0x%llx", (unsigned long long)loc.val);
    else if (is_float)
      snprintf(out, sz, "<fp-reg>");
    else if (is_uns)
      snprintf(out, sz, "%llu", (unsigned long long)loc.val);
    else
      snprintf(out, sz, "%lld", (long long)loc.val);
    return;
  }

  if (is_ptr) {
    uintptr_t pval = 0;
    if (peek_mem(pid, loc.addr, &pval, sizeof pval) != 0) {
      snprintf(out, sz, "<fault>");
      return;
    }
    if (strstr(type, "char") && pval) {
      char s[48];
      if (peek_mem(pid, pval, s, sizeof s - 1) == 0) {
        s[sizeof s - 1] = '\0';
        snprintf(out, sz, "0x%llx (\"%.*s\")", (unsigned long long)pval, 40, s);
      } else
        snprintf(out, sz, "0x%llx", (unsigned long long)pval);
    } else
      snprintf(out, sz, "0x%llx", (unsigned long long)pval);
    return;
  }

  if (is_float) {
    double dv = 0.0;
    if (peek_mem(pid, loc.addr, &dv, sizeof dv) == 0)
      snprintf(out, sz, "%g", dv);
    else
      snprintf(out, sz, "<fault>");
    return;
  }

  size_t bsz = 8;
  if (strstr(type, "long long"))
    bsz = 8;
  else if (strstr(type, "long"))
    bsz = 8;
  else if (!strcmp(type, "int64_t") || !strcmp(type, "uint64_t"))
    bsz = 8;
  else if (!strcmp(type, "int32_t") || !strcmp(type, "uint32_t"))
    bsz = 4;
  else if (strstr(type, "int"))
    bsz = 4;
  else if (strstr(type, "short"))
    bsz = 2;
  else if (strstr(type, "char"))
    bsz = 1;

  uint64_t raw = 0;
  if (peek_mem(pid, loc.addr, &raw, bsz) != 0) {
    snprintf(out, sz, "<fault>");
    return;
  }

  if (bsz == 1) {
    unsigned char uc = (unsigned char)raw;
    if (uc >= 0x20u && uc < 0x7fu)
      snprintf(out, sz, "'%c' (%u)", (char)uc, (unsigned)uc);
    else
      snprintf(out, sz, "%d", (int)(signed char)raw);
  } else if (is_uns) {
    snprintf(out, sz, "%llu", (unsigned long long)raw);
  } else {
    int64_t sv = (bsz == 4)   ? (int64_t)(int32_t)(uint32_t)raw
                 : (bsz == 2) ? (int64_t)(int16_t)(uint16_t)raw
                              : (int64_t)raw;
    snprintf(out, sz, "%lld", (long long)sv);
  }
}

static void resolve_type(Dwarf_Attribute *attr, char *out, size_t sz);

static void resolve_type_die(Dwarf_Die *die, char *out, size_t sz) {
  int tag = dwarf_tag(die);
  const char *n = dwarf_diename(die);

  switch (tag) {
  case DW_TAG_base_type:
  case DW_TAG_typedef:
  case DW_TAG_enumeration_type:
    snprintf(out, sz, "%s", n ? n : "?");
    break;
  case DW_TAG_structure_type:
    snprintf(out, sz, "struct %s", n ? n : "<anon>");
    break;
  case DW_TAG_union_type:
    snprintf(out, sz, "union %s", n ? n : "<anon>");
    break;
  case DW_TAG_pointer_type: {
    char base[200] = "void";
    Dwarf_Attribute sub;
    if (dwarf_attr(die, DW_AT_type, &sub))
      resolve_type(&sub, base, sizeof base);
    snprintf(out, sz, "%s *", base);
    break;
  }
  case DW_TAG_const_type: {
    char base[200] = "void";
    Dwarf_Attribute sub;
    if (dwarf_attr(die, DW_AT_type, &sub))
      resolve_type(&sub, base, sizeof base);
    snprintf(out, sz, "const %s", base);
    break;
  }
  case DW_TAG_volatile_type: {
    char base[200] = "void";
    Dwarf_Attribute sub;
    if (dwarf_attr(die, DW_AT_type, &sub))
      resolve_type(&sub, base, sizeof base);
    snprintf(out, sz, "volatile %s", base);
    break;
  }
  case DW_TAG_array_type: {
    char base[200] = "?";
    Dwarf_Attribute sub;
    if (dwarf_attr(die, DW_AT_type, &sub))
      resolve_type(&sub, base, sizeof base);
    snprintf(out, sz, "%s[]", base);
    break;
  }
  default:
    snprintf(out, sz, "%s", n ? n : "<unknown>");
    break;
  }
}

static void resolve_type(Dwarf_Attribute *attr, char *out, size_t sz) {
  Dwarf_Die die;
  if (dwarf_formref_die(attr, &die))
    resolve_type_die(&die, out, sz);
  else
    snprintf(out, sz, "?");
}

typedef struct RtVar {
  char name[256];
  char type[256];
  unsigned line;
  unsigned col;
  Dwarf_Op *ops; /* owned copy of location expression */
  size_t nops;
  struct RtVar *next;
} RtVar;

typedef struct RtFunc {
  char name[256];
  Dwarf_Addr low_pc;
  RtVar *vars;
  int nvar;
  struct RtFunc *next;
} RtFunc;

typedef struct {
  RtFunc *funcs;
  int nfunc;
} DwarfDB;

static void rtvar_free_list(RtVar *v) {
  while (v) {
    RtVar *nx = v->next;
    free(v->ops);
    free(v);
    v = nx;
  }
}
static void dwarfdb_free(DwarfDB *db) {
  if (!db)
    return;
  for (RtFunc *f = db->funcs; f;) {
    RtFunc *nx = f->next;
    rtvar_free_list(f->vars);
    free(f);
    f = nx;
  }
  free(db);
}

static void collect_scope(Dwarf_Die *scope, RtFunc *fe, Dwarf_Addr entry) {
  Dwarf_Die child;
  if (dwarf_child(scope, &child) != 0)
    return;

  do {
    int tag = dwarf_tag(&child);

    if (tag == DW_TAG_variable || tag == DW_TAG_formal_parameter) {
      const char *vname = dwarf_diename(&child);
      if (!vname)
        continue;

      Dwarf_Attribute loc_attr;
      if (!dwarf_attr(&child, DW_AT_location, &loc_attr))
        continue;

      /* Try location-list form first (covers both exprloc & loclist) */
      Dwarf_Op *ops = NULL;
      size_t nops = 0;
      int found = dwarf_getlocation_addr(&loc_attr, entry, &ops, &nops, 1);
      if (found <= 0 || nops == 0) {
        /* Fallback: single exprloc / block form */
        if (dwarf_getlocation(&loc_attr, &ops, &nops) != 0 || nops == 0)
          continue;
      }

      RtVar *ve = calloc(1, sizeof *ve);
      strncpy(ve->name, vname, sizeof ve->name - 1);

      Dwarf_Attribute ta;
      if (dwarf_attr(&child, DW_AT_type, &ta))
        resolve_type(&ta, ve->type, sizeof ve->type);
      else
        snprintf(ve->type, sizeof ve->type, "?");

      Dwarf_Attribute a;
      Dwarf_Word w;
      if (dwarf_attr(&child, DW_AT_decl_line, &a) &&
          dwarf_formudata(&a, &w) == 0)
        ve->line = (unsigned)w;
      if (dwarf_attr(&child, DW_AT_decl_column, &a) &&
          dwarf_formudata(&a, &w) == 0)
        ve->col = (unsigned)w;

      ve->ops = malloc(nops * sizeof *ops);
      memcpy(ve->ops, ops, nops * sizeof *ops);
      ve->nops = nops;

      ve->next = fe->vars;
      fe->vars = ve;
      fe->nvar++;
    }

    if (tag == DW_TAG_lexical_block)
      collect_scope(&child, fe, entry);

  } while (dwarf_siblingof(&child, &child) == 0);
}

static int collect_dwarf(const char *binary, DwarfDB *db) {
  int fd = open(binary, O_RDONLY);
  if (fd < 0) {
    perror("open");
    return -1;
  }

  Dwarf *dbg = dwarf_begin(fd, DWARF_C_READ);
  if (!dbg) {
    fprintf(stderr, "dwarf_begin: %s\n", dwarf_errmsg(-1));
    close(fd);
    return -1;
  }

  Dwarf_Off off = 0, next;
  size_t hdr_sz;

  while (dwarf_nextcu(dbg, off, &next, &hdr_sz, NULL, NULL, NULL) == 0) {
    Dwarf_Die cu;
    if (!dwarf_offdie(dbg, off + hdr_sz, &cu)) {
      off = next;
      continue;
    }

    Dwarf_Die child;
    if (dwarf_child(&cu, &child) != 0) {
      off = next;
      continue;
    }

    do {
      if (dwarf_tag(&child) != DW_TAG_subprogram)
        continue;

      const char *fname = dwarf_diename(&child);
      if (!fname)
        continue;

      /* Skip forward-declarations (no code) */
      Dwarf_Attribute decl;
      if (dwarf_attr(&child, DW_AT_declaration, &decl))
        continue;

      Dwarf_Addr low = 0;
      if (dwarf_entrypc(&child, &low) != 0)
        continue;

      RtFunc *fe = calloc(1, sizeof *fe);
      strncpy(fe->name, fname, sizeof fe->name - 1);
      fe->low_pc = low;

      collect_scope(&child, fe, low);

      fe->next = db->funcs;
      db->funcs = fe;
      db->nfunc++;

    } while (dwarf_siblingof(&child, &child) == 0);

    off = next;
  }

  dwarf_end(dbg);
  close(fd);
  return 0;
}

#define INT3 ((long)0xCC)

typedef struct {
  uintptr_t addr;
  long orig;
  RtFunc *func;
} Bp;

static uintptr_t get_slide(pid_t pid) {
  char path[64];
  snprintf(path, sizeof path, "/proc/%d/maps", pid);
  FILE *f = fopen(path, "r");
  if (!f)
    return 0;
  char line[512];
  uintptr_t slide = 0;
  while (fgets(line, sizeof line, f)) {
    unsigned long start;
    char perms[8];
    unsigned long foff;
    if (sscanf(line, "%lx-%*x %4s %lx", &start, perms, &foff) < 3)
      continue;
    if (perms[2] == 'x') {
      slide = start - foff;
      break;
    }
  }
  fclose(f);
  return slide;
}

static int bp_set(pid_t pid, uintptr_t addr, long *orig) {
  errno = 0;
  long w = ptrace(PTRACE_PEEKDATA, pid, (void *)addr, NULL);
  if (errno)
    return -1;
  *orig = w;
  return ptrace(PTRACE_POKEDATA, pid, (void *)addr,
                (void *)((w & ~(long)0xff) | INT3));
}

static int bp_restore(pid_t pid, uintptr_t addr, long orig) {
  return ptrace(PTRACE_POKEDATA, pid, (void *)addr, (void *)orig);
}

/* Single-step N times; sets *dead=1 if child exits unexpectedly */
static int step_n(pid_t pid, int n, int *dead, int *last_status) {
  *dead = 0;
  for (int i = 0; i < n; i++) {
    ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
    int st;
    waitpid(pid, &st, 0);
    *last_status = st;
    if (WIFEXITED(st) || WIFSIGNALED(st)) {
      *dead = 1;
      return -1;
    }
  }
  return 0;
}

VarList *runtime_analyze(const char *binary_path, char *const argv[]) {
  DwarfDB *db = calloc(1, sizeof *db);

  if (collect_dwarf(binary_path, db) != 0 || db->nfunc == 0) {
    fputs("runtime: no DWARF info — recompile with -g -O0\n", stderr);
    dwarfdb_free(db);
    return NULL;
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    dwarfdb_free(db);
    return NULL;
  }

  if (pid == 0) {
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);
    execv(binary_path, argv);
    perror("execv");
    _exit(1);
  }

  int status;
  waitpid(pid, &status, 0);
  if (!WIFSTOPPED(status)) {
    fputs("runtime: child did not stop\n", stderr);
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    dwarfdb_free(db);
    return NULL;
  }

  /* ASLR slide */
  uintptr_t slide = get_slide(pid);

  int nbp = 0;
  for (RtFunc *f = db->funcs; f; f = f->next)
    if (f->nvar > 0 && f->low_pc != 0)
      nbp++;

  Bp *bps = calloc(nbp ? nbp : 1, sizeof *bps);
  nbp = 0;

  for (RtFunc *f = db->funcs; f; f = f->next) {
    if (f->nvar == 0 || f->low_pc == 0)
      continue;
    uintptr_t addr = (uintptr_t)f->low_pc + slide;
    long orig;
    if (bp_set(pid, addr, &orig) == 0)
      bps[nbp++] = (Bp){addr, orig, f};
  }

  VarList *vl = varlist_new();
  int max_hits = 1000;

  ptrace(PTRACE_CONT, pid, NULL, NULL);

  while (max_hits-- > 0) {
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) || WIFSIGNALED(status))
      break;
    if (!WIFSTOPPED(status))
      break;

    int sig = WSTOPSIG(status);

    /* Pass non-SIGTRAP signals straight through */
    if (sig != SIGTRAP) {
      ptrace(PTRACE_CONT, pid, NULL, (void *)(long)sig);
      continue;
    }

    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) != 0)
      break;

    /* INT3 advances RIP by 1 — back up to the original address */
    uintptr_t hit = (uintptr_t)regs.rip - 1;

    Bp *bp = NULL;
    for (int i = 0; i < nbp; i++)
      if (bps[i].addr == hit) {
        bp = &bps[i];
        break;
      }

    if (!bp) {
      /* Spurious SIGTRAP (e.g. initial exec stop) — just continue */
      ptrace(PTRACE_CONT, pid, NULL, NULL);
      continue;
    }

    /* Restore original bytes and rewind RIP */
    bp_restore(pid, bp->addr, bp->orig);
    regs.rip = (unsigned long long)hit;
    ptrace(PTRACE_SETREGS, pid, NULL, &regs);

    int dead = 0;
    step_n(pid, 8, &dead, &status);

    if (!dead) {
      struct user_regs_struct live;
      if (ptrace(PTRACE_GETREGS, pid, NULL, &live) == 0) {
        for (RtVar *v = bp->func->vars; v; v = v->next) {
          VarInfo vi = {0};
          strncpy(vi.name, v->name, sizeof vi.name - 1);
          strncpy(vi.type, v->type, sizeof vi.type - 1);
          strncpy(vi.scope_name, bp->func->name, sizeof vi.scope_name - 1);
          vi.scope_kind = SCOPE_FUNCTION;
          vi.line = v->line;
          vi.col = v->col;

          LocResult loc = eval_ops(v->ops, v->nops, &live, slide);
          format_value(pid, v->type, loc, vi.value, sizeof vi.value);

          varlist_push(vl, &vi);
        }
      }
    }

    /* Re-arm the breakpoint for recursive / re-entrant calls */
    long orig2;
    if (bp_set(pid, bp->addr, &orig2) == 0)
      bp->orig = orig2;

    if (dead)
      break;
    ptrace(PTRACE_CONT, pid, NULL, NULL);
  }

  /* Clean up child */
  if (!WIFEXITED(status) && !WIFSIGNALED(status)) {
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
  }

  free(bps);
  dwarfdb_free(db);
  return vl;
}
