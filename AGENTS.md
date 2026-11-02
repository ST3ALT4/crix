# AGENTS.md — AI Agent Context for CRIX

> Read this file first. It contains everything you need to understand the project without scanning every source file.

## Project Summary

**CRIX (Code Runtime & Input eXplorer)** is a multi-language code execution visualizer. It takes a source file (C/C++, Rust, Python, etc.), extracts symbol and scope metadata via Language Server Protocol (LSP), traces execution step-by-step using a DAP debugger (e.g. `lldb-dap`), and produces structured data (variable snapshots, scope annotations, array mutation deltas, call stacks) that drive terminal timelines and animated Vulkan-based diagrams.

**Current state:** The CLI backend is functional with merged trace/summary modes and language-agnostic LSP symbol analysis. The Vulkan visual frontend is implemented.

## Architecture & Data Flow

```
Input: Source file (e.g. tests/bubble_sort.c)
  │
  ├─► LSP Analyzer (JSON-RPC over stdio)
  │     Auto-detects or uses --lsp-server (clangd, rust-analyzer, pyright...)
  │     Initializes session, queries textDocument/documentSymbol
  │     Extracts hierarchical symbol tree (variables, fields, constants, scopes)
  │
  ├─► Compiler Helper
  │     Auto-compiles source to temp binary with: clang/clang++ -g -O0
  │
  └─► Runtime Analyzer (DAP protocol via cppdap library)
        Forks lldb-dap as child process, communicates over pipes
        Sets breakpoint at main(), then single-steps through user code
        At each step: queries stack frames, scopes, variables
        Annotates steps with LSP scope and symbol kind metadata
        Builds ExecutionTrace (vector of StepRecords)
        │
        └─► Output:
              - Timeline (colored step-by-step terminal display with delta markers)
              - Trace JSON file (--out=path.json for frontend/tools)
              - Summary table / JSON (--summary [--format=table|json])
              - Vulkan Visualizer (--mode=visualize)
```

## File Map

### Headers (`include/`)

| File | Purpose | Key Types |
|------|---------|-----------|
| `lsp_analyzer.h` | Language Server Protocol (LSP) client interface | `LspSymbolInfo`, `SymbolList`, `LspConfig`, `detect_lsp_server()`, `lsp_analyze()`, `annotate_trace()` |
| `compiler_helper.h` | Auto-compile source to debug binary | `CompiledTarget` (move-only, RAII cleanup) |
| `runtime_analyze.h` | DAP-based runtime analysis interface | `RuntimeConfig`, `RuntimeResult`, `runtime_analyze_dap()` |
| `trace_model.h` | Execution trace data structures | `RuntimeValue`, `StepRecord`, `StepDelta`, `ArrayMutation`, `StackFrameInfo`, `ExecutionTrace` |
| `report.h` | Timeline and summary output functions | `report_timeline()`, `export_trace_json()`, `report_summary_table()`, `report_summary_json()` |
| `visualizer/vk_context.h` | Vulkan instance, device, swapchain & SDL3 window | `VulkanContext` |
| `visualizer/vk_pipeline.h` | Vulkan pipeline layout & Slang SPIR-V loader | `VulkanPipeline` |
| `visualizer/scene_renderer.h` | Instanced array element & bar renderer | `SceneRenderer`, `ElementInstanceData` |
| `visualizer/visual_model.h` | Decoupled, generic visual scene & node data model | `VisualNode`, `VisualProperty`, `VisualPointer`, `VisualScene` |
| `visualizer/topology_detector.h` | Universal DSA topology & pattern discovery engine | `DSATopology`, `TopologyNode`, `TopologyEdge`, `TopologyGraph`, `TopologyDetector` |
| `visualizer/anim_engine.h` | Modular, solid-color animation & layout engine | `AnimationEngine` |
| `visualizer/trace_visual_adapter.h` | Bridge adapter: CRIX StepRecord → VisualScene | `TraceVisualAdapter`, `DataStructureCandidate` |
| `visualizer/ui_overlay.h` | 3-Pane UI overlay, code view, stack trace, drop sidebar | `UIOverlay`, `UIPlaybackState` |
| `visualizer/visualizer_app.h` | Vulkan rendering loop & step playback engine | `VisualizerApp`, `run_visualizer()` |

### Sources (`src/`)

| File | What It Does |
|------|-------------|
| `main.cpp` | CLI argument parsing, mode dispatch (`trace`, `visualize`), LSP server auto-detection |
| `lsp_analyzer.cpp` | Minimal stdio JSON-RPC LSP client (forks server, initialize, documentSymbol, shutdown) |
| `compiler_helper.cpp` | Detects ELF vs source, compiles to /tmp with -g -O0, RAII cleanup of temp binary |
| `runtime_analyze.cpp` | Forks lldb-dap, DAP session over pipes, stepping loop, variable collection with child expansion |
| `trace_model.cpp` | RuntimeValue::to_string(), compute_delta() (diff variables, array mutations), JSON serialization |
| `report.cpp` | Colored timeline with ANSI codes, summary table and JSON, interactive step-through mode |
| `visualizer/anim_engine.cpp` | Modular rendering engine drawing solid visual cards, array cells, structs, and pointers |
| `visualizer/topology_detector.cpp` | Deterministic reflection of DSA topologies: Linked Lists, Binary Trees, 2D Grids, Sequences, and Call Trees |
| `visualizer/trace_visual_adapter.cpp` | Translates CRIX runtime step records into generic visual scenes and discovers inspectable data structures |
| `visualizer/vk_context.cpp` | Modern C++ Vulkan 1.2+ context, physical/logical device selection, swapchain |
| `visualizer/vk_pipeline.cpp` | C++ graphics pipeline, descriptor layouts, loads SPIR-V generated by Slang |
| `visualizer/scene_renderer.cpp` | Instanced SSBO data upload and quad bar rendering for array visualizer |
| `visualizer/ui_overlay.cpp` | 3-Pane UI (Executing Code, Stack Trace, Animation Canvas) with Drop Sidebar and Media Dock |
| `visualizer/visualizer_app.cpp` | Frame synchronization, presentation loop, animated step progression |

### Shaders (`shaders/`)

| File | Target | Description |
|------|--------|-------------|
| `shaders/bar_visualizer.slang` | SPIR-V (`.vert.spv`, `.frag.spv`) | Slang vertex & fragment shader for instanced bar rendering, mutation highlights, and edge borders |

### Test Programs (`tests/`)

| File | Exercises |
|------|-----------|
| `hello.c` | Globals, locals, function calls, if-blocks, char arrays, printf |
| `bubble_sort.c` | Array mutations, nested loops, swap pattern — primary DSA test case |
| `nested.c` | Recursion, typedefs, structs, enums, struct arrays, make_point() |
| `pointer_swap.c` | Pointer parameters, dereferencing, swap-by-pointer |
| `quicksort.c` | Partitioning, recursion, array mutations |
| `binary_search.c` | Loop convergence, comparisons, index bounds |
| `factorial.c` | Recursion stack and return values |

## Key Data Types

```cpp
// LSP Symbol information
struct LspSymbolInfo {
  string name;
  LspSymbolKind kind;
  string scope_name;
  int line, col;
  string detail;
};

// Runtime value (recursive — can hold arrays/structs)
struct RuntimeValue {
  ValueKind kind;  // PRIMITIVE, ARRAY, STRUCT, POINTER, OTHER
  string type_name, raw_value;
  vector<RuntimeValue> elements;              // array elements
  vector<pair<string, RuntimeValue>> fields;  // struct fields
};

// One step in the execution trace
struct StepRecord {
  int step_number, line, col;
  string file, function_name, stdout_chunk;
  vector<StackFrameInfo> call_stack;
  vector<pair<string, RuntimeValue>> variables;
  StepDelta delta;  // modified_vars + array_mutations vs previous step
  unordered_map<string, string> var_scope_names;  // varname -> LSP scope name
  unordered_map<string, int> var_symbol_kinds;   // varname -> LSP symbol kind
};

// The full trace
struct ExecutionTrace {
  string source_file;
  vector<StepRecord> steps;
  string to_json(bool pretty) const;
};
```

## Build System

- **Makefile** — builds with `clang++ -std=c++17`, compiles Slang shaders via `/opt/slang/bin/slangc` into SPIR-V.
- **Dependencies:** llvm, libdw/libelf, cppdap, jsoncpp, lldb-dap, vulkan, sdl3, slangc, imgui.
- **Build:** `make` builds shaders and produces `./crix`.

## CLI Modes

| Mode | LSP Analysis | Compile | DAP Trace | Output |
|------|:---:|:---:|:---:|--------|
| `trace` | Optional (auto/explicit/--no-lsp) | ✅ | ✅ | Step-by-step timeline + optional `--summary` table/json + `--out` JSON export |
| `visualize` | Optional (auto/explicit/--no-lsp) | ✅ | ✅ | Vulkan + Slang graphical window with modern web-style UI |

*(Note: legacy `static`, `runtime`, and `both` modes have been removed/merged into `trace`)*

## Known Issues & Resolved

1. **[RESOLVED] stddef.h / libclang dependency removed:** Migrated to language-agnostic LSP client (`clangd`, `rust-analyzer`, etc.) communicating over JSON-RPC stdio.
2. **[RESOLVED] Modes merged:** Merged `runtime` and `trace` into unified `trace` mode with `--summary` option; removed obsolete `static` and `both` modes.
3. **[RESOLVED] char[] display:** Char arrays are formatted as quoted string summaries.
4. **[RESOLVED] DAP banner in stdout:** Filtered OutputEvent to exclude adapter console messages.
5. **[RESOLVED] .gitignore added:** Ignore `.o`, `crix`, `compile_commands.json`, `.cache/`.

## Active Roadmap & Future Plans

- **[COMPLETED] 3-Pane Solid-Color UI & Drop Sidebar:**
  - Solid, flat 100% opacity color palette (Tailwind Slate/Zinc/Indigo).
  - 3-Pane synchronized layout: Pane 1 (Code Executing), Pane 2 (Stack Trace), Pane 3 (Modular Animation Canvas).
  - Modular, decoupled `AnimationEngine` and `VisualScene` data model.
  - Drop sidebar allowing the user to select and switch between observed data structures (arrays, structs, pointers, primitives).
  - Media-player bottom dock with play/pause, step controls, speed selector, and scrubbable timeline.
- **Algorithm-aware layouts:** Tree and graph node visualizers.
- **Multi-file support.**
