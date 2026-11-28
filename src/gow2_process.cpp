// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_process.h"

#include <algorithm>
#include <array>
#include <string>

namespace rex::glue::process {

std::string QuoteWindowsArgument(std::string_view argument) {
  if (!argument.empty() && argument.find_first_of(" \t\"") == std::string_view::npos)
    return std::string(argument);

  std::string result;
  result.reserve(argument.size() + 2);
  result.push_back('"');
  size_t backslashes = 0;
  for (const char character : argument) {
    if (character == '\\') {
      ++backslashes;
      continue;
    }
    if (character == '"') {
      result.append(backslashes * 2 + 1, '\\');
      result.push_back('"');
    } else {
      result.append(backslashes, '\\');
      result.push_back(character);
    }
    backslashes = 0;
  }
  result.append(backslashes * 2, '\\');
  result.push_back('"');
  return result;
}

}  // namespace rex::glue::process

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <limits>
#include <optional>

namespace rex::glue::process {
namespace {

class ScopedHandle {
 public:
  explicit ScopedHandle(HANDLE value = INVALID_HANDLE_VALUE) : value_(value) {}
  ~ScopedHandle() { Close(); }
  ScopedHandle(const ScopedHandle&) = delete;
  ScopedHandle& operator=(const ScopedHandle&) = delete;

  ScopedHandle(ScopedHandle&& other) noexcept : value_(other.Release()) {}

  ScopedHandle& operator=(ScopedHandle&& other) noexcept {
    if (this == &other) return *this;
    Close();
    value_ = other.Release();
    return *this;
  }

  HANDLE Get() const { return value_; }
  bool IsValid() const { return value_ != nullptr && value_ != INVALID_HANDLE_VALUE; }
  void Close() {
    if (IsValid()) CloseHandle(value_);
    value_ = INVALID_HANDLE_VALUE;
  }

 private:
  HANDLE Release() {
    const HANDLE value = value_;
    value_ = INVALID_HANDLE_VALUE;
    return value;
  }

  HANDLE value_;
};

class ScopedProcessAttributes {
 public:
  ScopedProcessAttributes() = default;
  ~ScopedProcessAttributes() {
    if (initialized_) DeleteProcThreadAttributeList(list_);
  }
  ScopedProcessAttributes(const ScopedProcessAttributes&) = delete;
  ScopedProcessAttributes& operator=(const ScopedProcessAttributes&) = delete;

  bool Initialize(const HANDLE* handles, size_t count) {
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    if (size == 0) return false;
    storage_.resize(size);
    list_ = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(storage_.data());
    if (!InitializeProcThreadAttributeList(list_, 1, 0, &size)) {
      list_ = nullptr;
      return false;
    }
    initialized_ = true;
    return UpdateProcThreadAttribute(list_, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                     const_cast<HANDLE*>(handles), count * sizeof(HANDLE), nullptr,
                                     nullptr) != FALSE;
  }

  PPROC_THREAD_ATTRIBUTE_LIST Get() const { return list_; }

 private:
  std::vector<std::byte> storage_;
  PPROC_THREAD_ATTRIBUTE_LIST list_ = nullptr;
  bool initialized_ = false;
};

std::string WindowsError(std::string_view operation, DWORD error) {
  return std::string(operation) + " failed (error " + std::to_string(error) + ")";
}

std::optional<std::wstring> Utf8ToWide(std::string_view text) {
  if (text.size() > static_cast<size_t>(std::numeric_limits<int>::max())) return std::nullopt;
  if (text.empty()) return std::wstring{};
  const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                         static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0) return std::nullopt;
  std::wstring result(static_cast<size_t>(length), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                          static_cast<int>(text.size()), result.data(), length) != length)
    return std::nullopt;
  return result;
}

std::optional<std::wstring> BuildCommandLine(const std::vector<std::string>& argv_utf8) {
  std::string command_line;
  for (const std::string& argument : argv_utf8) {
    if (!command_line.empty()) command_line.push_back(' ');
    command_line += QuoteWindowsArgument(argument);
  }
  return Utf8ToWide(command_line);
}

ScopedHandle OpenNull(DWORD access, SECURITY_ATTRIBUTES* security) {
  return ScopedHandle(CreateFileW(L"NUL", access, FILE_SHARE_READ | FILE_SHARE_WRITE, security,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
}

bool CreateOutputPipe(ScopedHandle* reader, ScopedHandle* writer, std::string* error) {
  SECURITY_ATTRIBUTES security{};
  security.nLength = sizeof(security);
  security.bInheritHandle = TRUE;
  HANDLE read_handle = INVALID_HANDLE_VALUE;
  HANDLE write_handle = INVALID_HANDLE_VALUE;
  if (!CreatePipe(&read_handle, &write_handle, &security, 0)) {
    *error = WindowsError("CreatePipe", GetLastError());
    return false;
  }
  *reader = ScopedHandle(read_handle);
  *writer = ScopedHandle(write_handle);
  if (SetHandleInformation(reader->Get(), HANDLE_FLAG_INHERIT, 0)) return true;
  *error = WindowsError("SetHandleInformation", GetLastError());
  return false;
}

bool SpawnWindows(const std::vector<std::string>& argv_utf8, HANDLE input, HANDLE output,
                  HANDLE error_output, PROCESS_INFORMATION* process, std::string* error) {
  if (argv_utf8.empty() || argv_utf8.front().empty()) {
    *error = "no executable specified";
    return false;
  }
  const auto executable = Utf8ToWide(argv_utf8.front());
  const auto command_line = BuildCommandLine(argv_utf8);
  if (!executable || !command_line) {
    *error = "invalid UTF-8 process arguments";
    return false;
  }
  std::wstring mutable_command_line = *command_line;
  std::array<HANDLE, 3> inherited_handles{};
  size_t inherited_count = 0;
  const std::array<HANDLE, 3> std_handles = {input, output, error_output};
  for (HANDLE handle : std_handles) {
    if (std::find(inherited_handles.begin(), inherited_handles.begin() + inherited_count, handle) !=
        inherited_handles.begin() + inherited_count) {
      continue;
    }
    inherited_handles[inherited_count++] = handle;
  }
  ScopedProcessAttributes attributes;
  if (!attributes.Initialize(inherited_handles.data(), inherited_count)) {
    *error = WindowsError("process handle list", GetLastError());
    return false;
  }
  STARTUPINFOEXW startup{};
  startup.StartupInfo.cb = sizeof(startup);
  startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  startup.StartupInfo.hStdInput = input;
  startup.StartupInfo.hStdOutput = output;
  startup.StartupInfo.hStdError = error_output;
  startup.lpAttributeList = attributes.Get();
  if (CreateProcessW(executable->c_str(), mutable_command_line.data(), nullptr, nullptr, TRUE,
                     CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
                     &startup.StartupInfo, process))
    return true;
  *error = WindowsError("CreateProcessW", GetLastError());
  return false;
}

void ReadOutput(HANDLE reader, size_t max_output, Result* result) {
  std::array<char, 8192> buffer{};
  for (;;) {
    DWORD count = 0;
    if (!ReadFile(reader, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) {
      const DWORD error = GetLastError();
      if (error == ERROR_BROKEN_PIPE) return;
      result->error = WindowsError("ReadFile", error);
      return;
    }
    if (count == 0) return;
    const size_t amount = std::min(max_output - result->output.size(), static_cast<size_t>(count));
    result->output.append(buffer.data(), amount);
    if (amount < count) result->output_truncated = true;
  }
}

void WaitForProcess(HANDLE handle, Result* result) {
  if (WaitForSingleObject(handle, INFINITE) != WAIT_OBJECT_0) {
    if (result->error.empty()) result->error = WindowsError("WaitForSingleObject", GetLastError());
    return;
  }
  DWORD exit_code = 0;
  if (!GetExitCodeProcess(handle, &exit_code)) {
    if (result->error.empty()) result->error = WindowsError("GetExitCodeProcess", GetLastError());
    return;
  }
  result->exit_code = static_cast<int>(exit_code);
}

bool OpenNullStdio(SECURITY_ATTRIBUTES* security, ScopedHandle* input, ScopedHandle* output,
                   ScopedHandle* error_output, std::string* error) {
  *input = OpenNull(GENERIC_READ, security);
  if (!input->IsValid()) {
    *error = WindowsError("CreateFileW", GetLastError());
    return false;
  }
  *output = OpenNull(GENERIC_WRITE, security);
  if (!output->IsValid()) {
    *error = WindowsError("CreateFileW", GetLastError());
    return false;
  }
  *error_output = OpenNull(GENERIC_WRITE, security);
  if (error_output->IsValid()) return true;
  *error = WindowsError("CreateFileW", GetLastError());
  return false;
}

}  // namespace

Result Run(const std::vector<std::string>& argv_utf8, size_t max_output) {
  Result result;
  SECURITY_ATTRIBUTES security{};
  security.nLength = sizeof(security);
  security.bInheritHandle = TRUE;
  ScopedHandle input;
  ScopedHandle null_output;
  ScopedHandle error_output;
  ScopedHandle pipe_reader;
  ScopedHandle pipe_writer;
  HANDLE output = INVALID_HANDLE_VALUE;
  if (max_output > 0) {
    if (!CreateOutputPipe(&pipe_reader, &pipe_writer, &result.error)) return result;
    input = OpenNull(GENERIC_READ, &security);
    if (!input.IsValid()) {
      result.error = WindowsError("CreateFileW", GetLastError());
      return result;
    }
    error_output = OpenNull(GENERIC_WRITE, &security);
    if (!error_output.IsValid()) {
      result.error = WindowsError("CreateFileW", GetLastError());
      return result;
    }
    output = pipe_writer.Get();
  } else {
    if (!OpenNullStdio(&security, &input, &null_output, &error_output, &result.error)) return result;
    output = null_output.Get();
  }

  PROCESS_INFORMATION process{};
  if (!SpawnWindows(argv_utf8, input.Get(), output, error_output.Get(), &process, &result.error))
    return result;
  result.started = true;
  ScopedHandle process_handle(process.hProcess);
  ScopedHandle thread_handle(process.hThread);
  if (max_output > 0) {
    pipe_writer.Close();
    ReadOutput(pipe_reader.Get(), max_output, &result);
    if (!result.error.empty()) pipe_reader.Close();
  }
  WaitForProcess(process_handle.Get(), &result);
  return result;
}

bool StartDetached(const std::vector<std::string>& argv_utf8) {
  SECURITY_ATTRIBUTES security{};
  security.nLength = sizeof(security);
  security.bInheritHandle = TRUE;
  ScopedHandle input;
  ScopedHandle output;
  ScopedHandle error_output;
  std::string error;
  if (!OpenNullStdio(&security, &input, &output, &error_output, &error)) return false;
  PROCESS_INFORMATION process{};
  if (!SpawnWindows(argv_utf8, input.Get(), output.Get(), error_output.Get(), &process, &error))
    return false;
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return true;
}

}  // namespace rex::glue::process

#elif defined(__linux__) && !defined(__ANDROID__)

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <system_error>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern char** environ;

namespace rex::glue::process {
namespace {

class FileDescriptor {
 public:
  explicit FileDescriptor(int descriptor = -1) : descriptor_(descriptor) {}
  ~FileDescriptor() { Close(); }
  FileDescriptor(const FileDescriptor&) = delete;
  FileDescriptor& operator=(const FileDescriptor&) = delete;

  int Get() const { return descriptor_; }
  void Set(int descriptor) {
    Close();
    descriptor_ = descriptor;
  }
  void Close() {
    if (descriptor_ < 0) return;
    close(descriptor_);
    descriptor_ = -1;
  }

 private:
  int descriptor_;
};

bool MoveAboveStdio(FileDescriptor* descriptor, std::string* error) {
  if (descriptor->Get() <= STDERR_FILENO) {
    const int moved = fcntl(descriptor->Get(), F_DUPFD, STDERR_FILENO + 1);
    if (moved < 0) {
      *error = std::strerror(errno);
      return false;
    }
    descriptor->Set(moved);
  }
  const int flags = fcntl(descriptor->Get(), F_GETFD);
  if (flags < 0 || fcntl(descriptor->Get(), F_SETFD, flags | FD_CLOEXEC) < 0) {
    *error = std::strerror(errno);
    return false;
  }
  return true;
}

class OutputPipe {
 public:
  bool Open(std::string* error) {
    int descriptors[2] = {-1, -1};
    if (pipe(descriptors) != 0) {
      *error = std::strerror(errno);
      return false;
    }
    reader_.Set(descriptors[0]);
    writer_.Set(descriptors[1]);
    return MoveAboveStdio(&reader_, error) && MoveAboveStdio(&writer_, error);
  }

  int Reader() const { return reader_.Get(); }
  int Writer() const { return writer_.Get(); }
  void CloseReader() { reader_.Close(); }
  void CloseWriter() { writer_.Close(); }

 private:
  FileDescriptor reader_;
  FileDescriptor writer_;
};

class SpawnActions {
 public:
  SpawnActions() : error_(posix_spawn_file_actions_init(&actions_)), initialized_(error_ == 0) {}
  ~SpawnActions() {
    if (initialized_) posix_spawn_file_actions_destroy(&actions_);
  }

  int Error() const { return error_; }
  posix_spawn_file_actions_t* Get() { return &actions_; }

 private:
  posix_spawn_file_actions_t actions_{};
  int error_;
  bool initialized_;
};

int ConfigureStdio(SpawnActions* actions, bool capture_output, const OutputPipe* output_pipe) {
  auto* file_actions = actions->Get();
  int error = posix_spawn_file_actions_addopen(file_actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
  if (error != 0) return error;
  if (capture_output) {
    error = posix_spawn_file_actions_addclose(file_actions, output_pipe->Reader());
    if (error == 0)
      error = posix_spawn_file_actions_adddup2(file_actions, output_pipe->Writer(), STDOUT_FILENO);
    if (error == 0) error = posix_spawn_file_actions_addclose(file_actions, output_pipe->Writer());
  } else {
    error = posix_spawn_file_actions_addopen(file_actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
  }
  if (error == 0)
    error = posix_spawn_file_actions_addopen(file_actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
  return error;
}

std::vector<char*> MakeArguments(const std::vector<std::string>& argv_utf8) {
  std::vector<char*> arguments;
  arguments.reserve(argv_utf8.size() + 1);
  for (const std::string& argument : argv_utf8)
    arguments.push_back(const_cast<char*>(argument.c_str()));
  arguments.push_back(nullptr);
  return arguments;
}

int Spawn(const std::vector<std::string>& argv_utf8, SpawnActions* actions, pid_t* child) {
  if (argv_utf8.empty() || argv_utf8.front().empty()) return EINVAL;
  const std::vector<char*> arguments = MakeArguments(argv_utf8);
  return posix_spawnp(child, argv_utf8.front().c_str(), actions->Get(), nullptr,
                      arguments.data(), environ);
}

void ReadOutput(int descriptor, size_t max_output, Result* result) {
  std::array<char, 8192> buffer{};
  for (;;) {
    const ssize_t count = read(descriptor, buffer.data(), buffer.size());
    if (count == 0) return;
    if (count < 0) {
      if (errno == EINTR) continue;
      result->error = std::strerror(errno);
      return;
    }
    const size_t amount = std::min(max_output - result->output.size(), static_cast<size_t>(count));
    result->output.append(buffer.data(), amount);
    if (amount < static_cast<size_t>(count)) result->output_truncated = true;
  }
}

void Reap(pid_t child) {
  int status = 0;
  pid_t waited = -1;
  do {
    waited = waitpid(child, &status, 0);
  } while (waited < 0 && errno == EINTR);
}

void WaitForChild(pid_t child, Result* result) {
  int status = 0;
  pid_t waited = -1;
  do {
    waited = waitpid(child, &status, 0);
  } while (waited < 0 && errno == EINTR);
  if (waited < 0) {
    if (result->error.empty()) result->error = std::string("waitpid failed: ") + std::strerror(errno);
    return;
  }
  if (WIFEXITED(status)) {
    result->exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result->exit_code = 128 + WTERMSIG(status);
  }
}

bool StartReaper(pid_t child) {
  try {
    std::thread([child] { Reap(child); }).detach();
  } catch (const std::system_error&) {
    Reap(child);
  }
  return true;
}

bool SpawnDetached(const std::vector<std::string>& argv_utf8) {
  if (argv_utf8.empty() || argv_utf8.front().empty()) return false;
  SpawnActions actions;
  if (actions.Error() != 0 || ConfigureStdio(&actions, false, nullptr) != 0) return false;
  pid_t child = -1;
  if (Spawn(argv_utf8, &actions, &child) != 0) return false;
  return StartReaper(child);
}

}  // namespace

Result Run(const std::vector<std::string>& argv_utf8, size_t max_output) {
  Result result;
  if (argv_utf8.empty() || argv_utf8.front().empty()) {
    result.error = "no executable specified";
    return result;
  }
  OutputPipe output_pipe;
  if (max_output > 0 && !output_pipe.Open(&result.error)) return result;
  SpawnActions actions;
  if (actions.Error() != 0) {
    result.error = std::strerror(actions.Error());
    return result;
  }
  const bool capture_output = max_output > 0;
  const int configuration_error = ConfigureStdio(&actions, capture_output, &output_pipe);
  if (configuration_error != 0) {
    result.error = std::strerror(configuration_error);
    return result;
  }
  pid_t child = -1;
  const int spawn_error = Spawn(argv_utf8, &actions, &child);
  if (spawn_error != 0) {
    result.error = std::strerror(spawn_error);
    return result;
  }
  result.started = true;
  if (capture_output) {
    output_pipe.CloseWriter();
    ReadOutput(output_pipe.Reader(), max_output, &result);
    if (!result.error.empty()) output_pipe.CloseReader();
  }
  WaitForChild(child, &result);
  return result;
}

bool StartDetached(const std::vector<std::string>& argv_utf8) {
  return SpawnDetached(argv_utf8);
}

}  // namespace rex::glue::process

#else

namespace rex::glue::process {

Result Run(const std::vector<std::string>&, size_t) { return {}; }

bool StartDetached(const std::vector<std::string>&) { return false; }

}  // namespace rex::glue::process

#endif