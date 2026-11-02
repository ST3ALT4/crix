# CRIX — Code Runtime & Input eXplorer

A multi-language visual code-execution tool that helps you **see how your code runs**, step by step.

Feed it a source file (C/C++, Rust, Python, etc.) and CRIX will extract symbol and scope metadata via Language Server Protocol (LSP), trace step-by-step through execution using a Debug Adapter Protocol (DAP) debugger (e.g. `lldb-dap`), and record variable values, array mutations, pointer transitions, function calls, and control flow to produce structured data driving terminal timelines and animated Vulkan visualizations.

---

## Modes

| Mode | What you get |
|------|-------------|
| `trace` | Step-by-step terminal execution timeline with variable snapshots, mutation deltas, and optional summary table/JSON (`--summary`) |
| `visualize` | Interactive Vulkan + Slang graphical window with modern web-style UI, Monaco-style synchronized code viewer, universal DSA topology layouts (Arrays, Linked Lists, Binary Trees, 2D Grids, Recursion Trees), Drop Sidebar, and scrubbable Media Dock |

---

## Visualizer Capabilities

The Vulkan graphical frontend features a modern, solid-color 3-pane synchronized layout:

- **Pane 1: Executing Code Viewer** — Synchronized source code view with line highlighting, active instruction pointer indicator, and scope tracking.
- **Pane 2: Call Stack & Variables** — Live hierarchical call stack frames, variable inspection, memory types, and LSP-enriched symbol kind badges (`Local`, `Parameter`, `Function`, `Struct`).
- **Pane 3: Universal DSA Canvas & Animation Engine** — Modular rendering engine with automated topology discovery:
  - **1D Sequences & Arrays**: Solid-color cells, mutation pulses, sliding window & two-pointer markers, comparison highlights.
  - **Linked Lists**: Dynamic node rendering with pointers (`head`, `curr`, `prev`) and animated edge relinking.
  - **Binary Trees & BSTs**: Hierarchical node-link layouts with parent-child pointers (`left`, `right`).
  - **2D Grids & DP Tables**: Row/col matrix layouts with active cell focus.
  - **Recursion Trees**: Live visualization of recursive invocation call frames and backtracking stacks.
- **Drop Sidebar**: Instantly switch and pin the visualization to any variable, pointer, data structure, or recursive call stack in scope.
- **Media Player Dock**: Scrubbable timeline slider, play/pause controls, step forward/backward, reset, and configurable playback speed (0.25x to 4.0x).

---

## Example: Tracing a Bubble Sort

```bash
./crix --mode=trace tests/bubble_sort.c --max-steps=10
```

```text
[Step 1] bubble_sort.c:16 in main()
  * data (int[4]) = [0, 0, 0, 0]
  * n (int) = 0

[Step 2] bubble_sort.c:17 in main()
  * data (int[4]) = [4, 1, 3, 2]

  ...

[Step 9] bubble_sort.c:9 in bubble_sort()
  * data (int[4]) = [1, 1, 3, 2]    ← swap happening

[Step 10] bubble_sort.c:11 in bubble_sort()
  * data (int[4]) = [1, 4, 3, 2]    ← swap complete
```

Or run visually with Vulkan hardware acceleration:
```bash
./crix --mode=visualize tests/bubble_sort.c
```

Or visualize dynamic node-link structures:
```bash
./crix --mode=visualize tests/linked_list.c
./crix --mode=visualize tests/binary_tree.c
./crix --mode=visualize tests/factorial.c
```

---

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
        ├─► Topology Detector (DSATopology discovery: Arrays, Trees, Lists, Grids, Call Trees)
        │
        └─► Output:
              - Timeline (colored step-by-step terminal display with delta markers)
              - Trace JSON file (--out=path.json for frontend/tools)
              - Summary table / JSON (--summary [--format=table|json])
              - Vulkan Visualizer (--mode=visualize)
```

---

## Build

### Dependencies

| Dependency | Purpose |
|------------|---------|
| `clang` / `clang++` | Host compiler (C++17) & debug binary generation |
| `clangd` / `rust-analyzer` | Language servers (LSP) for language-agnostic symbol analysis |
| `llvm` | llvm-config for build flags |
| `libdw` / `libelf` | DWARF debug info (elfutils) |
| [cppdap](https://github.com/google/cppdap) | Debug Adapter Protocol client library |
| `jsoncpp` | JSON-RPC serialization for the LSP client |
| `lldb-dap` | DAP server (runtime debugger) |
| `vulkan-headers` / `vulkan-icd-loader` | Vulkan 1.2+ API |
| `sdl3` | Window management, input handling, and swapchain surface creation |
| [slang](https://shader-slang.org/) (`slangc`) | Shading language compiler targeting SPIR-V |
| `imgui` | UI overlay with SDL3/Vulkan backends |

### Compile

```bash
make        # builds shaders and ./crix
make clean  # removes object files, compiled shaders, and binary
```

---

## Usage

```bash
./crix --mode=<trace|visualize> [options] <file> [-- program-args...]
```

### Options

| Flag | Default | Description |
|------|---------|-------------|
| `--mode=<mode>` | *(required)* | `trace` or `visualize` |
| `--summary` | off | Print final-state variable summary table/json after trace |
| `--format=<fmt>` | `table` | Summary output format: `table` or `json` (used with `--summary`) |
| `--out=<path.json>` | — | Export full trace JSON to file |
| `--max-steps=<N>` | `1000` | Max execution steps to record |
| `--interactive` | off | Pause at each step (press Enter to advance, `q` to finish) |
| `--adapter=<path>` | `lldb-dap` | Path to DAP adapter binary |
| `--lsp-server=<cmd>` | auto | Command to spawn language server (e.g. `clangd`, `rust-analyzer`, `"pyright --stdio"`) |
| `--no-lsp` | off | Disable LSP symbol analysis entirely (run pure DAP trace) |

### Language Server Auto-Detection

When `--lsp-server` is not specified, CRIX automatically selects a server based on the file extension:

| File Extension | Default LSP Server |
|---|---|
| `.c`, `.cpp`, `.cc`, `.cxx`, `.h`, `.hpp` | `clangd` |
| `.rs` | `rust-analyzer` |
| `.py` | `pyright --stdio` |
| `.go` | `gopls` |
| Other | `clangd` (best-effort) |

---

## Examples

```bash
# Trace — step-by-step execution timeline with auto-detected LSP (clangd)
./crix --mode=trace tests/bubble_sort.c

# Trace with final variable summary table
./crix --mode=trace --summary tests/hello.c

# Trace with final variable summary as JSON
./crix --mode=trace --summary --format=json tests/hello.c

# Export full trace JSON for visualizers and analysis
./crix --mode=trace tests/nested.c --out=trace.json

# Interactive trace — step through manually in the terminal
./crix --mode=trace tests/nested.c --interactive

# Explicit LSP server override
./crix --mode=trace --lsp-server=clangd tests/nested.c

# Skip LSP analysis entirely (pure runtime DAP tracing)
./crix --mode=trace --no-lsp tests/bubble_sort.c

# Graphical Visualizer — interactive Vulkan + Slang UI with animated diagrams
./crix --mode=visualize tests/bubble_sort.c

# Visualize linked lists, trees, and recursive calls
./crix --mode=visualize tests/linked_list.c
./crix --mode=visualize tests/binary_tree.c
./crix --mode=visualize tests/factorial.c
```

---

## Test Programs

| File | Exercises |
|------|-----------|
| `tests/hello.c` | Globals, locals, function calls, if-blocks, char arrays, printf |
| `tests/bubble_sort.c` | Array mutations, nested loops, swap pattern — primary DSA test case |
| `tests/nested.c` | Recursion, typedefs, structs, enums, struct arrays, make_point() |
| `tests/pointer_swap.c` | Pointer parameters, dereferencing, swap-by-pointer |
| `tests/quicksort.c` | Partitioning, recursion, array mutations |
| `tests/binary_search.c` | Loop convergence, comparisons, index bounds |
| `tests/factorial.c` | Recursion stack and return values |
| `tests/linked_list.c` | Dynamic singly linked list node creation, pointer traversal, node sum accumulator |
| `tests/binary_tree.c` | Dynamic binary tree node creation, pointer dereferencing, left/right subtree traversal |

---

## Roadmap

- [x] Language-agnostic LSP symbol and scope analyzer (`clangd`, `rust-analyzer`, etc.)
- [x] DAP runtime execution stepping and variable collection (`lldb-dap`)
- [x] Unified trace mode with array mutation deltas and final-state summaries
- [x] JSON trace export (`--out=path.json`) with LSP scope annotations
- [x] Vulkan + Slang graphics backend & shader pipeline
- [x] 3-Pane Solid-Color UI (Code Executing, Stack Trace, Animation Canvas)
- [x] Modular, data-structure agnostic Animation Engine with Drop Sidebar
- [x] Algorithm-aware graph and tree node layouts (Linked Lists, Binary Trees, 2D Grids, Recursion Trees)
- [ ] Interactive node drag-and-drop & canvas zooming / panning
- [ ] Multi-file project support

---

## License

TBD
