// Copyright 2010-2018, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/logging.h"

#ifdef OS_WIN
#define NO_MINMAX
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef OS_ANDROID
#include <android/log.h>
#endif  // OS_ANDROID

#ifdef OS_WIN
#include <codecvt>
#include <locale>
#endif  // OS_WIN

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#ifdef OS_ANDROID
#include "base/const.h"
#endif  // OS_ANDROID
#include "base/clock.h"
#include "base/flags.h"
#include "base/mutex.h"
#include "base/singleton.h"

DEFINE_bool(colored_log, true, "Enables colored log messages on tty devices");
DEFINE_bool(logtostderr,
            false,
            "log messages go to stderr instead of logfiles");
DEFINE_int32(v, 0, "verbose level");

namespace mozc {

#ifdef OS_ANDROID
namespace {
// In order to make logging.h independent from <android/log.h>, we use the
// raw number to define the following constants. Check the equality here
// just in case.
#define COMPARE_LOG_LEVEL(mozc_log_level, android_log_level)  \
    static_assert(static_cast<int>(mozc_log_level) ==         \
                  static_cast<int>(android_log_level),        \
                  "Checking Android log level constants.")
COMPARE_LOG_LEVEL(LOG_UNKNOWN, ANDROID_LOG_UNKNOWN);
COMPARE_LOG_LEVEL(LOG_DEFAULT, ANDROID_LOG_DEFAULT);
COMPARE_LOG_LEVEL(LOG_VERBOSE, ANDROID_LOG_VERBOSE);
COMPARE_LOG_LEVEL(LOG_DEBUG, ANDROID_LOG_DEBUG);
COMPARE_LOG_LEVEL(LOG_INFO, ANDROID_LOG_INFO);
COMPARE_LOG_LEVEL(LOG_WARNING, ANDROID_LOG_WARN);
COMPARE_LOG_LEVEL(LOG_ERROR, ANDROID_LOG_ERROR);
COMPARE_LOG_LEVEL(LOG_FATAL, ANDROID_LOG_FATAL);
COMPARE_LOG_LEVEL(LOG_SILENT, ANDROID_LOG_SILENT);
#undef COMPARE_LOG_LEVEL
}  // namespace
#endif  // OS_ANDROID

// Use the same implementation both for Opt and Debug.
string Logging::GetLogMessageHeader() {
#ifndef OS_ANDROID
  tm tm_time;
  Clock::GetCurrentTm(&tm_time);

  char buf[512];
  snprintf(buf, sizeof(buf),
           "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d %u "
#if defined(OS_NACL)
           "%p",
#elif defined(OS_LINUX)
           "%lu",
#elif defined(OS_MACOSX) && defined(__LP64__)
           "%llu",
#else  // OS_WIN or OS_MACOSX(32bit)
           "%u",
#endif
           1900 + tm_time.tm_year,
           1 + tm_time.tm_mon,
           tm_time.tm_mday,
           tm_time.tm_hour,
           tm_time.tm_min,
           tm_time.tm_sec,
#if defined(OS_WIN)
           ::GetCurrentProcessId(),
           ::GetCurrentThreadId()
#elif defined(OS_MACOSX)
           ::getpid(),
#ifdef __LP64__
           reinterpret_cast<uint64>(pthread_self())
#else  // __LP64__
           reinterpret_cast<uint32>(pthread_self())
#endif  // __LP64__
#elif defined(OS_NACL)
           ::getpid(),
           // pthread_self() returns __nc_basic_thread_data*.
           static_cast<void*>(pthread_self())
#else  // = OS_LINUX
           ::getpid(),
           // It returns unsigned long.
           pthread_self()
#endif
           );
  return buf;
#else  // OS_ANDROID
  // On Android, other records are not needed because they are added by
  // Android's logging framework.
  char buf[32];
  snprintf(buf, sizeof(buf),
           "%lu ",
           pthread_self());  // returns unsigned long.
  return buf;
#endif  // OS_ANDROID
}

#ifdef NO_LOGGING

void Logging::InitLogStream(const string &log_file_path) {
}

void Logging::CloseLogStream() {
}

std::ostream &Logging::GetWorkingLogStream() {
  // Never called.
  return *(new std::ostringstream);
}

void Logging::FinalizeWorkingLogStream(LogSeverity severity,
                                       std::ostream *working_stream) {
  // Never called.
  delete working_stream;
}

NullLogStream &Logging::GetNullLogStream() {
  return *(Singleton<NullLogStream>::get());
}

const char *Logging::GetLogSeverityName(LogSeverity severity) {
  return "";
}

const char *Logging::GetBeginColorEscapeSequence(LogSeverity severity) {
  return "";
}

const char *Logging::GetEndColorEscapeSequence() {
  return "";
}

int Logging::GetVerboseLevel() {
  return 0;
}

void Logging::SetVerboseLevel(int verboselevel) {
}

void Logging::SetConfigVerboseLevel(int verboselevel) {
}

#else   // NO_LOGGING

namespace {

class LogStreamImpl {
 public:
  LogStreamImpl();
  ~LogStreamImpl();

  void Init(const string &log_file_path);
  void Reset();

  int verbose_level() const {
    return std::max(FLAGS_v, config_verbose_level_);
  }

  void set_verbose_level(int level) {
    scoped_lock l(&mutex_);
    FLAGS_v = level;
  }

  void set_config_verbose_level(int level) {
    scoped_lock l(&mutex_);
    config_verbose_level_ = level;
  }

  bool support_color() const {
    return support_color_;
  }

  void Write(LogSeverity, const string &log);

 private:
  // Real backing log stream.
  // This is not thread-safe so must be guarded.
  // If std::cerr is real log stream, this is nullptr.
  std::ostream *real_log_stream_;
  int config_verbose_level_;
  bool support_color_;
  bool use_cerr_;
  Mutex mutex_;
};

void LogStreamImpl::Write(LogSeverity severity, const string &log) {
  scoped_lock l(&mutex_);
  if (use_cerr_) {
    std::cerr << log;
  } else {
#if defined(OS_ANDROID)
    __android_log_write(severity, kProductPrefix,
                        const_cast<char*>(log.c_str()));
#else  // OS_ANDROID
    // Since our logging mechanism is essentially singleton, it is indeed
    // possible that this method is called before |Logging::InitLogStream()|.
    // b/32360767 is an example, where |SystemUtil::GetLoggingDirectory()|
    // called as a preparation for |Logging::InitLogStream()| internally
    // triggered |LOG(ERROR)|.
    if (real_log_stream_) {
      *real_log_stream_ << log;
    }
#endif  // OS_ANDROID
  }
}

LogStreamImpl::LogStreamImpl() : real_log_stream_(nullptr) {
  Reset();
}

// Initializes real log stream.
// After initialization, use_cerr_ and real_log_stream_ become like following:
// OS, --logtostderr => use_cerr_, real_log_stream_
// Android, *     => false, nullptr
// NaCl,    *     => true,  nullptr
// Others,  true  => true,  nullptr
// Others,  false => true,  non-null
void LogStreamImpl::Init(const string &log_file_path) {
  scoped_lock l(&mutex_);
  Reset();

  if (use_cerr_) {
    // OS_NACL always reaches here.
    return;
  }
#if defined(OS_WIN)
  // On Windows, just create a stream.
  // Since Windows uses UTF-16 for internationalized file names, we should
  // convert the encoding of the given |log_file_path| from UTF-8 to UTF-16.
  // NOTE: To avoid circular dependency, |Util::UTF8ToWide| shouldn't be used
  // here.
  DCHECK_NE(log_file_path.size(), 0);
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf8_to_wide;
  real_log_stream_ = new std::ofstream(
      utf8_to_wide.from_bytes(log_file_path).c_str(), std::ios::app);
#elif !defined(OS_ANDROID)
  // On non-Android platform, change file mode in addition.
  // Android uses logcat instead of log file.
  DCHECK_NE(log_file_path.size(), 0);
  real_log_stream_ = new std::ofstream(log_file_path.c_str(), std::ios::app);
  ::chmod(log_file_path.c_str(), 0600);
#endif  // OS_ANDROID
  DCHECK(!use_cerr_ || !real_log_stream_);
}

void LogStreamImpl::Reset() {
  scoped_lock l(&mutex_);
  delete real_log_stream_;
  real_log_stream_ = nullptr;
  config_verbose_level_ = 0;
#if defined(OS_NACL)
    // In NaCl, we only use stderr to output logs.
    use_cerr_ = true;
    support_color_ = false;
#elif defined(OS_ANDROID)
    // Android uses Android's log library.
    use_cerr_ = false;
    support_color_ = false;
#elif defined(OS_WIN)
    // Coloring is disabled on windows
    // because cmd.exe doesn't support ANSI color escape sequences.
    // TODO(team): Considers to use SetConsoleTextAttribute on Windows.
    use_cerr_ = FLAGS_logtostderr;
    support_color_ = false;
#else  // OS_NACL, OS_ANDROID, OS_WIN
    use_cerr_ = FLAGS_logtostderr;
    support_color_ = use_cerr_ && FLAGS_colored_log
        && ::isatty(::fileno(stderr));
#endif  // OS_NACL, OS_ANDROID, OS_WIN
}

LogStreamImpl::~LogStreamImpl() {
  Reset();
}
}  // namespace

void Logging::InitLogStream(const string &log_file_path) {
  Singleton<LogStreamImpl>::get()->Init(log_file_path);
  std::ostream &stream = GetWorkingLogStream();
  stream << "Log file created at: " << Logging::GetLogMessageHeader();
  FinalizeWorkingLogStream(LogSeverity::LOG_INFO, &stream);
}

void Logging::CloseLogStream() {
  Singleton<LogStreamImpl>::get()->Reset();
}

std::ostream &Logging::GetWorkingLogStream() {
  return *(new std::ostringstream);
}

void Logging::FinalizeWorkingLogStream(LogSeverity severity,
                                       std::ostream *working_stream) {
  *working_stream << std::endl;
  Singleton<LogStreamImpl>::get()->Write(
      severity, static_cast<std::ostringstream*>(working_stream)->str());
  // The working stream is new'd in LogStreamImpl::GetWorkingLogStream().
  // Must be deleted by finalizer.
  delete working_stream;
}

NullLogStream &Logging::GetNullLogStream() {
  return *(Singleton<NullLogStream>::get());
}

namespace {
// ANSI Color escape sequences.
// FYI: Other escape sequences are here.
// Black:   "\x1b[30m"
// Green    "\x1b[32m"
// Blue:    "\x1b[34m"
// Magenta: "\x1b[35m"
// White    "\x1b[37m"
const char *kClearEscapeSequence   = "\x1b[0m";
const char *kRedEscapeSequence     = "\x1b[31m";
const char *kYellowEscapeSequence  = "\x1b[33m";
const char *kCyanEscapeSequence    = "\x1b[36m";

const struct SeverityProperty {
 public:
  const char *label;
  const char *color_escape_sequence;
} kSeverityProperties[] = {
#ifdef OS_ANDROID
  { "UNKNOWN", kCyanEscapeSequence },
  { "DEFAULT", kCyanEscapeSequence },
  { "VERBOSE", kCyanEscapeSequence },
  { "DEBUG",   kCyanEscapeSequence },
  { "INFO",    kCyanEscapeSequence },
  { "WARNING", kYellowEscapeSequence },
  { "ERROR",   kRedEscapeSequence },
  { "FATAL",   kRedEscapeSequence },
  { "SILENT",  kCyanEscapeSequence },
#else
  { "INFO",    kCyanEscapeSequence },
  { "WARNING", kYellowEscapeSequence },
  { "ERROR",   kRedEscapeSequence },
  { "FATAL",   kRedEscapeSequence },
#endif  // OS_ANDROID
};
}  // namespace

const char *Logging::GetLogSeverityName(LogSeverity severity) {
  return kSeverityProperties[severity].label;
}

const char *Logging::GetBeginColorEscapeSequence(LogSeverity severity) {
  if (Singleton<LogStreamImpl>::get()->support_color()) {
    return kSeverityProperties[severity].color_escape_sequence;
  }
  return "";
}

const char *Logging::GetEndColorEscapeSequence() {
  if (Singleton<LogStreamImpl>::get()->support_color()) {
    return kClearEscapeSequence;
  }
  return "";
}

int Logging::GetVerboseLevel() {
  return Singleton<LogStreamImpl>::get()->verbose_level();
}

void Logging::SetVerboseLevel(int verboselevel) {
  Singleton<LogStreamImpl>::get()->set_verbose_level(verboselevel);
}

void Logging::SetConfigVerboseLevel(int verboselevel) {
  Singleton<LogStreamImpl>::get()->set_config_verbose_level(verboselevel);
}
#endif  // NO_LOGGING

LogFinalizer::LogFinalizer(LogSeverity severity)
    : severity_(severity) {}

LogFinalizer::~LogFinalizer() {
  Logging::FinalizeWorkingLogStream(severity_, working_stream_);
  if (severity_ >= LOG_FATAL) {
    // On windows, call exception handler to
    // make stack trace and minidump
#ifdef OS_WIN
    ::RaiseException(::GetLastError(), EXCEPTION_NONCONTINUABLE, 0, nullptr);
#else
    mozc::Logging::CloseLogStream();
    exit(-1);
#endif
  }
}

void LogFinalizer::operator&(std::ostream& working_stream) {
  working_stream_ = &working_stream;
}

void NullLogFinalizer::OnFatal() {
#ifdef OS_WIN
  ::RaiseException(::GetLastError(), EXCEPTION_NONCONTINUABLE, 0, nullptr);
#else
  exit(-1);
#endif
}

}       // namespace mozc
