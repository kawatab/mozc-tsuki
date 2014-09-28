// Copyright 2010-2014, Google Inc.
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

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#ifdef OS_ANDROID
#include <sstream>
#endif  // OS_ANDROID
#include <string>

#ifdef OS_ANDROID
#include "base/const.h"
#endif  // OS_ANDROID
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/flags.h"
#include "base/mutex.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/util.h"

DEFINE_bool(colored_log, true, "Enables colored log messages on tty devices");
DEFINE_bool(logtostderr,
            false,
            "log messages go to stderr instead of logfiles");

// Even if log_dir is modified in the middle of the process, the
// logging directory will not be changed because the logging stream is
// initialized in the very early initialization stage.
DEFINE_string(log_dir,
              "",
              "If specified, logfiles are written into this directory "
              "instead of the default logging directory.");
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
  Util::GetCurrentTm(&tm_time);

  char buf[512];
  snprintf(buf, sizeof(buf),
           "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d %u "
#if defined(__native_client__)
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
#elif defined(__native_client__)
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

void Logging::InitLogStream(const char *argv0) {
}

void Logging::CloseLogStream() {
}

ostream &Logging::GetLogStream() {
  return cerr;
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
  void Init(const char *argv0);
  void Close();

  ostream *stream() {
     return (stream_ == NULL) ? &cerr : stream_;
  }

  int verbose_level() const {
    return max(FLAGS_v, config_verbose_level_);
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

  LogStreamImpl();
  virtual ~LogStreamImpl();

 private:
  ostream *stream_;
  int config_verbose_level_;
  bool support_color_;
  Mutex mutex_;
};

LogStreamImpl::LogStreamImpl()
    : stream_(NULL), config_verbose_level_(0), support_color_(false) {
}

void LogStreamImpl::Init(const char *argv0) {
  scoped_lock l(&mutex_);
  if (stream_ != NULL) {
    return;
  }
#ifdef __native_client__
    // In NaCl, we only use stderr to output logs.
    stream_ = &cerr;
#else
  if (FLAGS_logtostderr) {
    stream_ = &cerr;
#ifndef OS_WIN
    // Disables on windows because cmd.exe doesn't support ANSI color escape
    // sequences.
    // TODO(team): Considers to use SetConsoleTextAttribute on Windows.
    support_color_ = FLAGS_colored_log && isatty(fileno(stderr));
#endif  // OS_WIN
  } else {
#ifdef OS_ANDROID
    // Use ostringstream to output logging messages by android's logging
    // framework.
    stream_ = new ostringstream();
#else
#ifdef OS_WIN
    const char *slash = ::strrchr(argv0, '\\');
#else
    const char *slash = ::strrchr(argv0, '/');
#endif
    const char *program_name = (slash == NULL) ? argv0 : slash + 1;
    const string log_base = string(program_name) + ".log";
    const string log_dir =
        FLAGS_log_dir.empty() ? SystemUtil::GetLoggingDirectory() :
                                FLAGS_log_dir;
    const string filename = FileUtil::JoinPath(log_dir, log_base);
    stream_ = new OutputFileStream(filename.c_str(), ios::app);
#ifndef OS_WIN
    ::chmod(filename.c_str(), 0600);
#endif
#endif  // OS_ANDROID
  }
#endif  // __native_client__

  *stream_ << "Log file created at: "
           << Logging::GetLogMessageHeader() << endl;
  *stream_ << "Program name: " << argv0 << endl;
}

void LogStreamImpl::Close() {
  scoped_lock l(&mutex_);
  if (stream_ != &cerr) {
    delete stream_;
  }
  config_verbose_level_ = 0;
  stream_ = NULL;
}

LogStreamImpl::~LogStreamImpl() {
  Close();
}
}  // namespace

void Logging::InitLogStream(const char *argv0) {
  Singleton<LogStreamImpl>::get()->Init(argv0);
}

void Logging::CloseLogStream() {
  Singleton<LogStreamImpl>::get()->Close();
}

ostream &Logging::GetLogStream() {
  return *(Singleton<LogStreamImpl>::get()->stream());
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
  mozc::Logging::GetLogStream() << endl;
#ifdef OS_ANDROID
  ostream *log_stream = &mozc::Logging::GetLogStream();
  if (log_stream != &cerr) {
    ostringstream *log_stringstream = static_cast<ostringstream*>(log_stream);
     __android_log_write(severity_,
                         kProductPrefix,
                         log_stringstream->str().c_str());
    log_stringstream->str("");
  }
#endif  // OS_ANDROID
  if (severity_ >= LOG_FATAL) {
    // On windows, call exception handler to
    // make stack trace and minidump
#ifdef OS_WIN
    ::RaiseException(::GetLastError(),
                     EXCEPTION_NONCONTINUABLE,
                     NULL, NULL);
#else
    mozc::Logging::CloseLogStream();
    exit(-1);
#endif
  }
}

void LogFinalizer::operator&(ostream&) {}

void NullLogFinalizer::OnFatal() {
#ifdef OS_WIN
  ::RaiseException(::GetLastError(),
                   EXCEPTION_NONCONTINUABLE,
                   NULL, NULL);
#else
  exit(-1);
#endif
}

}       // namespace mozc
