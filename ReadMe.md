# CRIX — Code Runtime & Input eXplorer

A visual code-execution tool that helps you **see how your code runs**, step by step.

Feed it a C source file and CRIX will trace through every line — showing variable values, array mutations, function calls, and control flow — producing structured data that can drive animated visualizations of algorithms and data structures.

> **Vision:** Point CRIX at a sorting algorithm, and watch an animated diagram show you exactly how the array transforms at each step. The Vulkan-based visual frontend is planned for a future milestone; the current CLI backend produces the trace data that will power it.

---

## What It Does Today

CRIX has four modes:

| Mode | What you get |
|------|-------------|
| `static` | Variable table from AST analysis (names, types, scopes, locations) |
| `runtime` | Variable table enriched with final runtime values from the debugger |
| `trace` | Step-by-step execution timeline with variable snapshots and array mutation deltas |
| `both` | Trace timeline + summary variable table |
| `visualize` | Interactive Vulkan + Slang graphical window animating execution & array mutations |

### Example: Tracing a Bubble Sort

```
$ ./crix --mode=trace tests/bubble_sort.c --max-steps=10
```

```
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

Or run visually:
```bash
./crix --mode=visualize tests/bubble_sort.c
```

---

## Build

### Dependencies

| Dependency | Purpose |
|------------|---------|
| `clang` / `clang++` | Compiler (C++17) |
| `libclang-dev` | AST parsing for static analysis |
| `llvm-dev` | llvm-config for build flags |
| `libdw-dev` / `libelf-dev` | DWARF debug info (elfutils) |
| [cppdap](https://github.com/nicknisi/cppdap) | Debug Adapter Protocol client library |
| `lldb-dap` | DAP server (runtime debugger) |
| `vulkan-headers` / `vulkan-icd-loader` | Vulkan API & modern C++ Vulkan-Hpp bindings |
| `glfw3` | Window management and swapchain surface creation |
| [slang](https://shader-slang.org/) (`slangc`) | Next-gen shading language compiler targeting SPIR-V |
| `imgui` | Modern GUI overlay for code view, player timeline, and variable watch |

### Compile

```bash
make        # builds ./crix
make clean  # remove object files and binary
```

---

## Usage

```
./crix --mode=<static|runtime|trace|both> [options] <file.c> [-- program-args...]
```

### Options

| Flag | Default | Description |
|------|---------|-------------|
| `--mode=<mode>` | *(required)* | `static`, `runtime`, `trace`, or `both` |
| `--format=<fmt>` | `table` | Output format: `table` or `json` |
| `--out=<path>` | — | Export trace to JSON file |
| `--max-steps=<N>` | `1000` | Max execution steps to record |
| `--interactive` | off | Pause at each step (press Enter to advance) |
| `--adapter=<path>` | `lldb-dap` | Path to DAP adapter binary |

### Examples

```bash
# Static analysis — see all variables, types, and scopes
./crix --mode=static tests/hello.c

# Runtime analysis — see final variable values after execution
./crix --mode=runtime tests/hello.c

# Trace — step-by-step execution timeline
./crix --mode=trace tests/bubble_sort.c --out=trace.json

# Interactive trace — step through manually
./crix --mode=both tests/nested.c --interactive

# Graphical Visualizer — interactive Vulkan + Slang UI with animated diagrams
./crix --mode=visualize tests/bubble_sort.c

# JSON output for programmatic use
./crix --mode=static tests/hello.c --format=json
```

---

## Test Programs

| File | Tests |
|------|-------|
| `tests/hello.c` | Globals, locals, function calls, if-blocks, char arrays |
| `tests/bubble_sort.c` | Array mutations, nested loops, swap pattern |
| `tests/nested.c` | Recursion, structs, enums, struct arrays |
| `tests/pointer_swap.c` | Pointer parameters, dereferencing |

---

## Roadmap

- [x] Static AST analysis (libclang)
- [x] Runtime variable inspection (DAP/lldb-dap)
- [x] Step-by-step execution tracing with deltas
- [x] JSON trace export for frontend consumption
- [x] Vulkan + Slang graphics backend & shader pipeline
- [ ] Modern Web-style UI & Interactive Visualizer (Monaco-style code view, step narration banner, annotated array cards, media-player timeline)
- [ ] Algorithm-aware visualizations (trees, graphs, pointer links)
- [ ] Multi-file project support

---

## License

TBD
