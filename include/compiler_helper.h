#ifndef COMPILER_HELPER_H
#define COMPILER_HELPER_H

#include <string>
#include <vector>

struct CompiledTarget {
  std::string binary_path;
  std::string source_path;
  bool is_temporary = false;
  bool success = false;
  std::string error_message;

  CompiledTarget() = default;

  // Move only — prevent accidental double-cleanup
  CompiledTarget(const CompiledTarget &) = delete;
  CompiledTarget &operator=(const CompiledTarget &) = delete;

  CompiledTarget(CompiledTarget &&o) noexcept
      : binary_path(std::move(o.binary_path)),
        source_path(std::move(o.source_path)),
        is_temporary(o.is_temporary),
        success(o.success),
        error_message(std::move(o.error_message)) {
    o.is_temporary = false; // prevent donor from cleaning up
  }

  CompiledTarget &operator=(CompiledTarget &&o) noexcept {
    if (this != &o) {
      cleanup();
      binary_path = std::move(o.binary_path);
      source_path = std::move(o.source_path);
      is_temporary = o.is_temporary;
      success = o.success;
      error_message = std::move(o.error_message);
      o.is_temporary = false;
    }
    return *this;
  }

  void cleanup();
  ~CompiledTarget() { cleanup(); }
};

CompiledTarget prepare_target(const std::string &input_path, const std::vector<std::string> &extra_compile_flags = {});

#endif // COMPILER_HELPER_H
