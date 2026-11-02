#include "lsp_analyzer.h"
#include "trace_model.h"

#include <json/json.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace {

static std::string path_to_uri(const std::string& path) {
  // Percent-encode characters that are illegal in URI path segments.
  // We only handle the common problematic ones for local paths.
  std::string out = "file://";
  for (unsigned char c : path) {
    if (c == ' ')       { out += "%20"; }
    else if (c == '#')  { out += "%23"; }
    else if (c == '?')  { out += "%3F"; }
    else                { out += static_cast<char>(c); }
  }
  return out;
}

static std::string dir_of(const std::string& path) {
  size_t pos = path.find_last_of('/');
  return (pos == std::string::npos) ? "." : path.substr(0, pos);
}

static std::string lang_id_for(const std::string& path) {
  auto dot = path.rfind('.');
  if (dot == std::string::npos) return "plaintext";
  std::string ext = path.substr(dot + 1);
  if (ext == "c")                                       return "c";
  if (ext == "cpp" || ext == "cc" || ext == "cxx")      return "cpp";
  if (ext == "h"   || ext == "hpp")                     return "cpp";
  if (ext == "rs")                                      return "rust";
  if (ext == "py")                                      return "python";
  if (ext == "go")                                      return "go";
  if (ext == "js"  || ext == "jsx")                     return "javascript";
  if (ext == "ts"  || ext == "tsx")                     return "typescript";
  return "plaintext";
}

static bool read_file_content(const std::string& path, std::string& out) {
  std::ifstream f(path, std::ios::in);
  if (!f.is_open()) return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

// ── LSP transport ─────────────────────────────────────────────────────────────

// Read exactly `n` bytes from fd, blocking with per-call timeout_ms for
// each individual read(). Returns total bytes read; < n means timeout/error.
static size_t read_exact(int fd, char* buf, size_t n, int timeout_ms) {
  size_t total = 0;
  while (total < n) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    int sel = select(fd + 1, &rfds, nullptr, nullptr, &tv);
    if (sel <= 0) return total;  // timeout or error
    ssize_t n_read = read(fd, buf + total, n - total);
    if (n_read <= 0) return total;
    total += static_cast<size_t>(n_read);
  }
  return total;
}

// Read one character with timeout. Returns true on success.
static bool read_char(int fd, char& c, int timeout_ms) {
  return read_exact(fd, &c, 1, timeout_ms) == 1;
}

// Read a complete LSP message (header + body) from fd.
static bool lsp_read_msg(int fd, Json::Value& out, int timeout_ms) {
  int content_length = -1;

  // Parse HTTP-style headers until blank line
  while (true) {
    std::string line;
    char c;
    while (read_char(fd, c, timeout_ms)) {
      if (c == '\n') break;
      if (c != '\r') line += c;
    }
    if (line.empty()) {
      if (content_length >= 0) break;  // blank line = end of headers
      continue;
    }
    const std::string CL = "Content-Length: ";
    if (line.rfind(CL, 0) == 0) {
      try { content_length = std::stoi(line.substr(CL.size())); } catch (...) {}
    }
  }

  if (content_length <= 0) return false;

  std::string body(content_length, '\0');
  if (read_exact(fd, body.data(), static_cast<size_t>(content_length), timeout_ms)
      != static_cast<size_t>(content_length)) {
    return false;
  }

  Json::CharReaderBuilder builder;
  std::string errors;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  return reader->parse(body.c_str(), body.c_str() + body.size(), &out, &errors);
}

// Serialize and write an LSP message to fd.
static bool lsp_write_msg(int fd, const Json::Value& msg) {
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  builder["commentStyle"] = "None";
  const std::string body = Json::writeString(builder, msg);
  const std::string frame =
      "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;

  const char* ptr = frame.c_str();
  size_t remaining = frame.size();
  while (remaining > 0) {
    ssize_t n = write(fd, ptr, remaining);
    if (n <= 0) return false;
    ptr       += n;
    remaining -= static_cast<size_t>(n);
  }
  return true;
}

// Wait for a JSON-RPC response whose "id" matches `req_id`.
// Silently discards notifications and mismatched responses.
static bool lsp_wait_for(int fd, int req_id, Json::Value& response,
                          int timeout_ms, int max_messages = 40) {
  for (int i = 0; i < max_messages; ++i) {
    Json::Value msg;
    if (!lsp_read_msg(fd, msg, timeout_ms)) return false;
    // A response has "id" and no "method"
    if (msg.isMember("id") && !msg.isMember("method")) {
      // id may be int or string depending on server
      bool id_match = false;
      if (msg["id"].isInt())    id_match = (msg["id"].asInt() == req_id);
      if (msg["id"].isString()) id_match = (msg["id"].asString() == std::to_string(req_id));
      if (id_match) { response = msg; return true; }
    }
    // Otherwise: notification or different response → keep draining
  }
  return false;
}

// ── Symbol extraction ─────────────────────────────────────────────────────────

// LSP SymbolKinds that represent variable-like declarations
static bool is_variable_kind(int k) {
  return k == static_cast<int>(LspSymbolKind::Variable)    // 13
      || k == static_cast<int>(LspSymbolKind::Field)        // 8
      || k == static_cast<int>(LspSymbolKind::Constant)     // 14
      || k == static_cast<int>(LspSymbolKind::EnumMember)   // 22
      || k == static_cast<int>(LspSymbolKind::Property);    // 7
}

// LSP SymbolKinds that create a named scope for their children
static bool is_scope_kind(int k) {
  return k == static_cast<int>(LspSymbolKind::Function)    // 12
      || k == static_cast<int>(LspSymbolKind::Method)       // 6
      || k == static_cast<int>(LspSymbolKind::Constructor)  // 9
      || k == static_cast<int>(LspSymbolKind::Class)        // 5
      || k == static_cast<int>(LspSymbolKind::Struct)       // 23
      || k == static_cast<int>(LspSymbolKind::Enum)         // 10
      || k == static_cast<int>(LspSymbolKind::Namespace)    // 3
      || k == static_cast<int>(LspSymbolKind::Module);      // 2
}

static void collect_symbols(const Json::Value& sym,
                             const std::string& parent_scope,
                             SymbolList& result) {
  if (!sym.isObject()) return;

  const int kind         = sym.get("kind", 0).asInt();
  const std::string name = sym.get("name", "").asString();

  int line = 0, col = 0;
  if (sym.isMember("range") && sym["range"].isMember("start")) {
    line = sym["range"]["start"].get("line", 0).asInt();
    col  = sym["range"]["start"].get("character", 0).asInt();
  }

  if (is_variable_kind(kind)) {
    LspSymbolInfo info;
    info.name       = name;
    info.kind       = static_cast<LspSymbolKind>(kind);
    info.scope_name = parent_scope;
    info.line       = line;
    info.col        = col;
    info.detail     = sym.get("detail", "").asString();
    result.push_back(std::move(info));
  }

  // Determine scope name passed down to children
  const std::string child_scope = is_scope_kind(kind) ? name : parent_scope;

  const Json::Value& children = sym["children"];
  if (children.isArray()) {
    for (const auto& child : children) {
      collect_symbols(child, child_scope, result);
    }
  }
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

std::string detect_lsp_server(const std::string& filepath) {
  auto dot = filepath.rfind('.');
  if (dot == std::string::npos) return "clangd";
  const std::string ext = filepath.substr(dot + 1);

  if (ext == "c"   || ext == "cpp" || ext == "cc"  ||
      ext == "cxx" || ext == "h"   || ext == "hpp")   return "clangd";
  if (ext == "rs")                                     return "rust-analyzer";
  if (ext == "py")                                     return "pyright --stdio";
  if (ext == "go")                                     return "gopls";
  if (ext == "js"  || ext == "jsx")                   return "typescript-language-server --stdio";
  if (ext == "ts"  || ext == "tsx")                   return "typescript-language-server --stdio";
  return "clangd"; // best-effort fallback
}

SymbolList lsp_analyze(const std::string& filepath, const LspConfig& cfg) {
  SymbolList result;

  // ── Resolve absolute path ──────────────────────────────────────────────────
  std::string abs_path = filepath;
  if (abs_path.empty() || abs_path[0] != '/') {
    char buf[PATH_MAX];
    if (realpath(filepath.c_str(), buf)) abs_path = buf;
  }

  const std::string file_uri = path_to_uri(abs_path);
  const std::string root_uri = path_to_uri(dir_of(abs_path));

  // ── Read source ────────────────────────────────────────────────────────────
  std::string source;
  if (!read_file_content(abs_path, source)) {
    std::cerr << "[lsp] Cannot read source file: " << abs_path << "\n";
    return result;
  }

  // ── Parse server command (may contain spaces, e.g. "pyright --stdio") ─────
  std::vector<std::string> cmd_parts;
  {
    std::istringstream ss(cfg.server_cmd);
    std::string tok;
    while (ss >> tok) cmd_parts.push_back(tok);
  }
  if (cmd_parts.empty()) return result;

  // ── Fork + exec language server ────────────────────────────────────────────
  int to_srv[2], from_srv[2];
  if (pipe(to_srv) != 0 || pipe(from_srv) != 0) {
    std::cerr << "[lsp] pipe() failed\n";
    return result;
  }

  pid_t pid = fork();
  if (pid < 0) {
    std::cerr << "[lsp] fork() failed\n";
    close(to_srv[0]); close(to_srv[1]);
    close(from_srv[0]); close(from_srv[1]);
    return result;
  }

  if (pid == 0) {
    // Child: wire stdin/stdout, silence stderr, exec server
    close(to_srv[1]);
    close(from_srv[0]);
    dup2(to_srv[0],   STDIN_FILENO);
    dup2(from_srv[1], STDOUT_FILENO);
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }
    close(to_srv[0]);
    close(from_srv[1]);

    std::vector<const char*> argv;
    for (const auto& p : cmd_parts) argv.push_back(p.c_str());
    argv.push_back(nullptr);
    execvp(argv[0], const_cast<char* const*>(argv.data()));
    _exit(127);
  }

  // Parent
  close(to_srv[0]);
  close(from_srv[1]);
  const int wfd = to_srv[1];
  const int rfd = from_srv[0];
  const int T   = cfg.timeout_ms;

  auto cleanup = [&]() {
    close(wfd);
    close(rfd);
    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
  };

  int next_id = 1;

  // ── 1. initialize ──────────────────────────────────────────────────────────
  {
    Json::Value req;
    req["jsonrpc"] = "2.0";
    req["id"]      = next_id;
    req["method"]  = "initialize";
    auto& p = req["params"];
    p["processId"] = static_cast<int>(getpid());
    p["clientInfo"]["name"]    = "crix";
    p["clientInfo"]["version"] = "0.1";
    p["rootUri"] = root_uri;
    p["capabilities"]["textDocument"]["documentSymbol"]
     ["hierarchicalDocumentSymbolSupport"] = true;
    p["capabilities"]["workspace"]["symbol"]["resolveSupport"]["properties"]
     = Json::arrayValue;

    if (!lsp_write_msg(wfd, req)) { cleanup(); return result; }
  }

  {
    Json::Value resp;
    if (!lsp_wait_for(rfd, next_id, resp, T)) {
      std::cerr << "[lsp] initialize timed out (" << cfg.server_cmd << ")\n";
      cleanup(); return result;
    }
    ++next_id;
  }

  // ── 2. initialized notification ────────────────────────────────────────────
  {
    Json::Value notif;
    notif["jsonrpc"] = "2.0";
    notif["method"]  = "initialized";
    notif["params"]  = Json::objectValue;
    lsp_write_msg(wfd, notif);
  }

  // ── 3. textDocument/didOpen ────────────────────────────────────────────────
  {
    Json::Value notif;
    notif["jsonrpc"] = "2.0";
    notif["method"]  = "textDocument/didOpen";
    auto& td = notif["params"]["textDocument"];
    td["uri"]        = file_uri;
    td["languageId"] = lang_id_for(filepath);
    td["version"]    = 1;
    td["text"]       = source;
    lsp_write_msg(wfd, notif);
  }

  // ── 4. textDocument/documentSymbol ────────────────────────────────────────
  const int sym_req_id = next_id++;
  {
    Json::Value req;
    req["jsonrpc"] = "2.0";
    req["id"]      = sym_req_id;
    req["method"]  = "textDocument/documentSymbol";
    req["params"]["textDocument"]["uri"] = file_uri;
    lsp_write_msg(wfd, req);
  }

  {
    Json::Value resp;
    if (!lsp_wait_for(rfd, sym_req_id, resp, T)) {
      std::cerr << "[lsp] documentSymbol timed out (" << cfg.server_cmd << ")\n";
    } else {
      const Json::Value& sym_result = resp["result"];
      if (sym_result.isArray()) {
        for (const auto& sym : sym_result) {
          collect_symbols(sym, "<global>", result);
        }
      }
    }
  }

  // ── 5. shutdown + exit ────────────────────────────────────────────────────
  {
    Json::Value shut;
    shut["jsonrpc"] = "2.0";
    shut["id"]      = next_id++;
    shut["method"]  = "shutdown";
    shut["params"]  = Json::nullValue;
    lsp_write_msg(wfd, shut);

    Json::Value ex;
    ex["jsonrpc"] = "2.0";
    ex["method"]  = "exit";
    ex["params"]  = Json::nullValue;
    lsp_write_msg(wfd, ex);
  }

  cleanup();
  return result;
}

void annotate_trace(ExecutionTrace& trace, const SymbolList& symbols) {
  if (symbols.empty() || trace.steps.empty()) return;

  // Build name → symbol index map. When multiple symbols share a name
  // (e.g. a field and a local), the one whose scope_name matches the
  // current function name takes priority; otherwise last-write wins.
  std::unordered_map<std::string, const LspSymbolInfo*> by_name;
  for (const auto& sym : symbols) {
    by_name[sym.name] = &sym;  // last entry wins (good enough heuristic)
  }

  // Build a secondary map keyed by "scope::name" for precise lookup
  std::unordered_map<std::string, const LspSymbolInfo*> by_scoped_name;
  for (const auto& sym : symbols) {
    by_scoped_name[sym.scope_name + "::" + sym.name] = &sym;
  }

  for (auto& step : trace.steps) {
    const std::string& fn = step.function_name;

    for (const auto& [var_name, val] : step.variables) {
      // Try function-scoped lookup first
      auto it = by_scoped_name.find(fn + "::" + var_name);
      if (it == by_scoped_name.end()) {
        // Fall back to global-scoped
        it = by_scoped_name.find("<global>::" + var_name);
      }
      if (it == by_scoped_name.end()) {
        // Fall back to simple name lookup
        auto it2 = by_name.find(var_name);
        if (it2 != by_name.end()) {
          step.var_scope_names[var_name]  = it2->second->scope_name;
          step.var_symbol_kinds[var_name] = static_cast<int>(it2->second->kind);
        }
        continue;
      }
      step.var_scope_names[var_name]  = it->second->scope_name;
      step.var_symbol_kinds[var_name] = static_cast<int>(it->second->kind);
    }
  }
}
