#include "compiler_helper.h"
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include <iostream>
#include <cstdlib>

static bool is_elf_file(const std::string &path) {
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open()) return false;
  char magic[4] = {0};
  f.read(magic, 4);
  return (magic[0] == 0x7f && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F');
}

static bool ends_with(const std::string &str, const std::string &suffix) {
  if (str.length() < suffix.length()) return false;
  return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

void CompiledTarget::cleanup() {
  if (is_temporary && !binary_path.empty()) {
    unlink(binary_path.c_str());
    binary_path.clear();
  }
}

CompiledTarget prepare_target(const std::string &input_path, const std::vector<std::string> &extra_compile_flags) {
  CompiledTarget target;
  target.source_path = input_path;

  // Check if input_path exists
  struct stat st;
  if (stat(input_path.c_str(), &st) != 0) {
    target.success = false;
    target.error_message = "File does not exist: " + input_path;
    return target;
  }

  // If already an ELF binary
  if (is_elf_file(input_path)) {
    target.binary_path = input_path;
    target.is_temporary = false;
    target.success = true;
    return target;
  }

  // Determine compiler based on extension
  std::string compiler;
  if (ends_with(input_path, ".c")) {
    compiler = "clang";
  } else if (ends_with(input_path, ".cpp") || ends_with(input_path, ".cc") || ends_with(input_path, ".cxx")) {
    compiler = "clang++";
  } else {
    // Attempt clang by default
    compiler = "clang";
  }

  char temp_template[] = "/tmp/crix_bin_XXXXXX";
  int fd = mkstemp(temp_template);
  if (fd < 0) {
    target.success = false;
    target.error_message = "Failed to create temporary file for compilation.";
    return target;
  }
  close(fd);

  std::string out_bin = temp_template;
  std::string cmd = compiler + " -g -O0 -fno-omit-frame-pointer \"" + input_path + "\" -o \"" + out_bin + "\"";
  for (const auto &flag : extra_compile_flags) {
    cmd += " " + flag;
  }
  cmd += " 2>&1";

  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    target.success = false;
    target.error_message = "Failed to run compiler: " + cmd;
    unlink(out_bin.c_str());
    return target;
  }

  char buf[256];
  std::string output;
  while (fgets(buf, sizeof(buf), pipe)) {
    output += buf;
  }
  int code = pclose(pipe);

  if (code != 0) {
    target.success = false;
    target.error_message = "Compilation failed:\n" + output;
    unlink(out_bin.c_str());
    return target;
  }

  target.binary_path = out_bin;
  target.is_temporary = true;
  target.success = true;
  return target;
}
