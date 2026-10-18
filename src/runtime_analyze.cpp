#include "runtime_analyze.h"
#include "trace_model.h"

#include <dap/dap.h>
#include <dap/io.h>
#include <dap/protocol.h>
#include <dap/session.h>
#include <dap/typeof.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <unordered_set>
#include <chrono>

namespace dap {
struct CrixLaunchRequest : public LaunchRequest {
  string program;
  array<string> args;
  string cwd;
  boolean stopOnEntry = false;
};

DAP_DECLARE_STRUCT_TYPEINFO(CrixLaunchRequest);
DAP_IMPLEMENT_STRUCT_TYPEINFO_EXT(
    CrixLaunchRequest,
    LaunchRequest,
    "launch",
    DAP_FIELD(program, "program"),
    DAP_FIELD(args, "args"),
    DAP_FIELD(cwd, "cwd"),
    DAP_FIELD(stopOnEntry, "stopOnEntry")
);
} // namespace dap

static std::string get_basename(const std::string &path) {
  size_t idx = path.find_last_of("/\\");
  if (idx == std::string::npos) return path;
  return path.substr(idx + 1);
}

static RuntimeValue parse_variable(
    dap::Session *session,
    const dap::Variable &var,
    int depth = 0,
    int max_depth = 2) {
  RuntimeValue rv;
  rv.type_name = var.type.value("");
  rv.raw_value = var.value;

  if (var.variablesReference > 0 && depth < max_depth) {
    dap::VariablesRequest child_req;
    child_req.variablesReference = var.variablesReference;
    auto child_future = session->send(child_req);
    auto child_res = child_future.get();

    if (!child_res.error && !child_res.response.variables.empty()) {
      bool is_array = false;
      if (rv.type_name.find('[') != std::string::npos ||
          (!child_res.response.variables.empty() && child_res.response.variables[0].name.rfind("[", 0) == 0)) {
        is_array = true;
      }

      if (is_array) {
        rv.kind = ValueKind::ARRAY;
        for (const auto &c : child_res.response.variables) {
          rv.elements.push_back(parse_variable(session, c, depth + 1, max_depth));
        }
      } else {
        rv.kind = ValueKind::STRUCT;
        for (const auto &c : child_res.response.variables) {
          rv.fields.push_back({c.name, parse_variable(session, c, depth + 1, max_depth)});
        }
      }
      return rv;
    }
  }

  if (rv.type_name.find('*') != std::string::npos) {
    rv.kind = ValueKind::POINTER;
  } else {
    rv.kind = ValueKind::PRIMITIVE;
  }
  return rv;
}

RuntimeResult runtime_analyze_dap(
    const std::string &binary_path,
    const std::string &source_file,
    const std::vector<std::string> &prog_args,
    const VarList &static_vars,
    const RuntimeConfig &config) {
  RuntimeResult result;
  result.trace.source_file = source_file.empty() ? binary_path : source_file;
  result.final_vars = static_vars;

  dap::initialize();

  int to_dap[2], from_dap[2];
  if (pipe(to_dap) != 0 || pipe(from_dap) != 0) {
    result.error_message = "Failed to create IPC pipes for DAP adapter.";
    return result;
  }

  pid_t pid = fork();
  if (pid < 0) {
    result.error_message = "Failed to fork DAP adapter process.";
    return result;
  }

  if (pid == 0) {
    // Child: lldb-dap
    close(to_dap[1]);
    close(from_dap[0]);

    dup2(to_dap[0], STDIN_FILENO);
    dup2(from_dap[1], STDOUT_FILENO);

    close(to_dap[0]);
    close(from_dap[1]);

    execlp(config.adapter_path.c_str(), config.adapter_path.c_str(), nullptr);
    _exit(127);
  }

  // Parent
  close(to_dap[0]);
  close(from_dap[1]);

  FILE *in_fp = fdopen(from_dap[0], "r");
  FILE *out_fp = fdopen(to_dap[1], "w");
  if (!in_fp || !out_fp) {
    result.error_message = "Failed to open stream file descriptors for DAP.";
    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
    return result;
  }

  auto rw = dap::ReaderWriter::create(
      dap::file(in_fp, true),
      dap::file(out_fp, true)
  );

  auto session = dap::Session::create();
  session->bind(rw);

  std::mutex mtx;
  std::condition_variable cv;
  bool is_stopped = false;
  bool is_terminated = false;
  int64_t stopped_tid = 0;
  std::string stop_reason;
  std::string accumulated_stdout;

  session->registerHandler([&](const dap::InitializedEvent &) {
    // Breakpoint at main to start tracing from user entry
    dap::SetFunctionBreakpointsRequest fbp;
    dap::FunctionBreakpoint bp;
    bp.name = "main";
    fbp.breakpoints = {bp};
    session->send(fbp).get();

    session->send(dap::ConfigurationDoneRequest{});
  });

  session->registerHandler([&](const dap::StoppedEvent &ev) {
    std::lock_guard<std::mutex> lk(mtx);
    is_stopped = true;
    stopped_tid = ev.threadId.value(0);
    stop_reason = ev.reason;
    cv.notify_one();
  });

  session->registerHandler([&](const dap::OutputEvent &ev) {
    std::lock_guard<std::mutex> lk(mtx);
    // Only capture program stdout, not DAP adapter console messages
    std::string category = ev.category.value("");
    if (category == "stdout" || category == "") {
      accumulated_stdout += ev.output;
    }
  });

  session->registerHandler([&](const dap::TerminatedEvent &) {
    std::lock_guard<std::mutex> lk(mtx);
    is_terminated = true;
    cv.notify_one();
  });

  session->registerHandler([&](const dap::ExitedEvent &) {
    std::lock_guard<std::mutex> lk(mtx);
    is_terminated = true;
    cv.notify_one();
  });

  // Initialize
  dap::InitializeRequest init_req;
  init_req.clientID = "crix";
  init_req.adapterID = "lldb";
  init_req.linesStartAt1 = true;
  init_req.columnsStartAt1 = true;
  auto init_res = session->send(init_req).get();
  if (init_res.error) {
    result.error_message = "DAP Initialize failed: " + init_res.error.message;
    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
    return result;
  }

  // Launch
  dap::CrixLaunchRequest launch_req;
  launch_req.program = binary_path;
  launch_req.args = prog_args;
  launch_req.stopOnEntry = false;
  auto launch_res = session->send(launch_req).get();
  if (launch_res.error) {
    result.error_message = "DAP Launch failed: " + launch_res.error.message;
    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
    return result;
  }

  std::string src_basename = get_basename(source_file);
  std::unordered_map<std::string, std::string> latest_var_values;

  size_t step_count = 0;
  int consecutive_non_user_steps = 0;

  while (step_count < config.max_steps) {
    std::unique_lock<std::mutex> lk(mtx);
    cv.wait_for(lk, std::chrono::seconds(2), [&] { return is_stopped || is_terminated; });

    if (is_terminated) {
      break;
    }
    if (!is_stopped) {
      // Timeout waiting for stop
      break;
    }

    is_stopped = false;
    int64_t tid = stopped_tid;
    std::string step_stdout = accumulated_stdout;
    accumulated_stdout.clear();
    lk.unlock();

    // Query stack trace
    dap::StackTraceRequest st_req;
    st_req.threadId = tid;
    auto st_res = session->send(st_req).get();
    if (st_res.error || st_res.response.stackFrames.empty()) {
      // Step over and continue
      dap::NextRequest nr;
      nr.threadId = tid;
      session->send(nr);
      continue;
    }

    const auto &top = st_res.response.stackFrames[0];
    std::string frame_file = top.source.value({}).name.value("");
    std::string frame_path = top.source.value({}).path.value("");

    bool in_user_source = false;
    if (!src_basename.empty()) {
      if (frame_file == src_basename || get_basename(frame_path) == src_basename) {
        in_user_source = true;
      }
    } else if (!frame_file.empty() && frame_path.find("/usr/") == std::string::npos) {
      in_user_source = true;
    }

    if (!in_user_source) {
      consecutive_non_user_steps++;
      if (consecutive_non_user_steps > 30) {
        // Prevent getting lost in runtime startup/shutdown
        break;
      }
      dap::StepOutRequest so;
      so.threadId = tid;
      session->send(so);
      continue;
    }

    consecutive_non_user_steps = 0;
    step_count++;

    StepRecord record;
    record.step_number = static_cast<int>(step_count);
    record.file = frame_file.empty() ? src_basename : frame_file;
    record.line = static_cast<int>(top.line);
    record.col = static_cast<int>(top.column);
    record.function_name = top.name;
    record.stdout_chunk = step_stdout;

    std::unordered_set<std::string> seen_vars;

    for (size_t frame_idx = 0; frame_idx < st_res.response.stackFrames.size(); ++frame_idx) {
      const auto &sf = st_res.response.stackFrames[frame_idx];
      StackFrameInfo sfi;
      sfi.id = sf.id;
      sfi.name = sf.name;
      sfi.file = sf.source.value({}).name.value("");
      sfi.line = static_cast<int>(sf.line);
      sfi.col = static_cast<int>(sf.column);

      std::string sf_path = sf.source.value({}).path.value("");

      // Only query variables for user frames within direct call chain
      bool is_user_frame = sf.source.has_value() && !sfi.file.empty() && 
                           (sfi.file == src_basename || sf_path.find("/usr/") == std::string::npos);

      if (is_user_frame && frame_idx < 2) {
        dap::ScopesRequest sc_req;
        sc_req.frameId = sf.id;
        auto sc_res = session->send(sc_req).get();
        if (!sc_res.error) {
          for (const auto &scope : sc_res.response.scopes) {
            if (scope.name == "Locals" || scope.name == "Arguments") {
              dap::VariablesRequest vr;
              vr.variablesReference = scope.variablesReference;
              auto vr_res = session->send(vr).get();
              if (!vr_res.error) {
                for (const auto &v : vr_res.response.variables) {
                  RuntimeValue rv = parse_variable(session.get(), v, 0, 2);
                  sfi.variables.push_back({v.name, rv});
                  latest_var_values[v.name] = rv.to_string();

                  // Include in record.variables (top frame takes precedence)
                  if (frame_idx == 0 || seen_vars.find(v.name) == seen_vars.end()) {
                    record.variables.push_back({v.name, rv});
                    seen_vars.insert(v.name);
                  }
                }
              }
            }
          }
        }
      }
      record.call_stack.push_back(sfi);
    }

    // Compute deltas from previous step
    if (!result.trace.steps.empty()) {
      record.delta = compute_delta(result.trace.steps.back(), record);
    } else {
      for (const auto &p : record.variables) {
        record.delta.modified_vars.push_back(p.first);
      }
    }

    result.trace.steps.push_back(record);

    // Advance execution
    if (config.step_into_user_functions) {
      dap::StepInRequest step_in;
      step_in.threadId = tid;
      session->send(step_in);
    } else {
      dap::NextRequest step_next;
      step_next.threadId = tid;
      session->send(step_next);
    }
  }

  // Disconnect cleanly
  dap::DisconnectRequest disc;
  disc.terminateDebuggee = true;
  session->send(disc).get();

  // Tear down in correct order: close transport, destroy session, then terminate DAP
  rw->close();
  session.reset();
  waitpid(pid, nullptr, 0);
  dap::terminate();

  // Populate final variable table from latest observed values
  for (auto &v : result.final_vars) {
    auto it = latest_var_values.find(v.name);
    if (it != latest_var_values.end()) {
      v.value = it->second;
    }
  }

  result.success = true;
  return result;
}
