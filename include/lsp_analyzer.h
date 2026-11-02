#ifndef LSP_ANALYZER_H
#define LSP_ANALYZER_H

#include <string>
#include <vector>
#include <unordered_map>

// Forward declaration — avoid circular include with trace_model.h
struct ExecutionTrace;

// LSP SymbolKind values (per LSP spec §3.16)
enum class LspSymbolKind : int {
  File         = 1,
  Module       = 2,
  Namespace    = 3,
  Package      = 4,
  Class        = 5,
  Method       = 6,
  Property     = 7,
  Field        = 8,
  Constructor  = 9,
  Enum         = 10,
  Interface    = 11,
  Function     = 12,
  Variable     = 13,
  Constant     = 14,
  String       = 15,
  Number       = 16,
  Boolean      = 17,
  Array        = 18,
  Object       = 19,
  Key          = 20,
  Null         = 21,
  EnumMember   = 22,
  Struct       = 23,
  Event        = 24,
  Operator     = 25,
  TypeParameter= 26,
};

struct LspSymbolInfo {
  std::string name;
  LspSymbolKind kind;
  std::string scope_name;  // parent symbol name (e.g. "bubble_sort" or "<global>")
  int line = 0;            // 0-based (LSP native)
  int col  = 0;
  std::string detail;      // type hint string when the server provides it
};

using SymbolList = std::vector<LspSymbolInfo>;

struct LspConfig {
  std::string server_cmd = "clangd"; // command (and args) for the language server
  int timeout_ms = 5000;             // per-read timeout in milliseconds
};

// Auto-detect the LSP server command from the file's extension.
// Returns e.g. "clangd", "rust-analyzer", "pyright --stdio", "gopls".
std::string detect_lsp_server(const std::string& filepath);

// Launch the language server, query textDocument/documentSymbol, and return
// the flat list of variable-like symbols extracted from the DocumentSymbol tree.
// On any failure the function prints a warning and returns an empty list (non-fatal).
SymbolList lsp_analyze(const std::string& filepath, const LspConfig& cfg = {});

// Annotate every StepRecord in `trace` with LSP scope/kind metadata for each
// variable, matched by name from `symbols`. Call after runtime_analyze_dap().
void annotate_trace(ExecutionTrace& trace, const SymbolList& symbols);

#endif // LSP_ANALYZER_H
