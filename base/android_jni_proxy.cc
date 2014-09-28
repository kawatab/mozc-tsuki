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

#include "base/android_jni_proxy.h"

#include <jni.h>

#include "base/logging.h"
#include "base/mutex.h"
#include "base/scoped_ptr.h"
#include "net/http_client_common.h"

namespace {

// To invoke Java's method, we need to ensure that the current thread is
// attached to Java VM. This class manages it, and provides a way to access
// JNIEnv instance, which is an interface to call Java's methods.
class ScopedJavaThreadAttacher {
 public:
  explicit ScopedJavaThreadAttacher(JavaVM *jvm) : jvm_(jvm) {
    jni_env_ = GetJNIEnv(jvm_, &attached_);
  }

  ~ScopedJavaThreadAttacher() {
    if (attached_ && jvm_ != NULL) {
      jvm_->DetachCurrentThread();
    }
  }

  // Returned JNIEnv may be invalidated when this instance is destructed.
  JNIEnv *mutable_jni_env() {
    return jni_env_;
  }

 private:
  JavaVM *jvm_;
  JNIEnv *jni_env_;
  bool attached_;

  // Returns JNIEnv instance which attached to the current thread, and
  // stores true into attached, when this thread is newly attached to the
  // Java VM (i.e., the caller has responsibility to detach from the Java VM.
  static JNIEnv* GetJNIEnv(JavaVM *jvm, bool *attached) {
    *attached = false;

    if (jvm == NULL) {
      // In case jvm is accidentally NULL, we do nothing, instead of
      // terminating the thread.
      LOG(ERROR) << "Given jvm is null.";
      return NULL;
    }

    // First, try to get JNIEnv instance attached to the current thread.
    JNIEnv *env;
    jint result = jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (result == JNI_EDETACHED) {
      // This thread is not attached to Java VM. So try to attach.
      JavaVMAttachArgs attach_args = { JNI_VERSION_1_6, NULL, NULL };
      result = jvm->AttachCurrentThread(&env, &attach_args);
      if (result != JNI_OK) {
        LOG(ERROR) << "Failed to attach Java thread.";
        return NULL;
      }

      *attached = true;
    }

    if (result != JNI_OK) {
      LOG(ERROR) << "Failed to get JNIEnv.";
      return NULL;
    }

    return env;
  }

  DISALLOW_COPY_AND_ASSIGN(ScopedJavaThreadAttacher);
};

// Creates jbyteArray instance containing the data.
jbyteArray BufferToJByteArray(
    JNIEnv *env, const void *data, const size_t size) {
  jbyteArray result = env->NewByteArray(size);
  CHECK(result);
  env->SetByteArrayRegion(
      result, 0, size, reinterpret_cast<const jbyte*>(data));
  return result;
}

// Copies the contents of the given source to buf, and store the size into
// buf_size.
// Retruns true, if finished successfully. Otherwise, false.
bool CopyJByteArrayToBuf(JNIEnv *env, const jbyteArray &source,
                         void *buf, size_t *buf_size) {
  const jsize size = env->GetArrayLength(source);
  if (size > *buf_size) {
    return false;
  }
  env->GetByteArrayRegion(source, 0, size, reinterpret_cast<jbyte*>(buf));
  *buf_size = size;
  return true;
}

void AssignJByteArrayToString(JNIEnv *env, const jbyteArray &source,
                              string *dest) {
  size_t size = env->GetArrayLength(source);
  scoped_ptr<char[]> buf(new char[size]);
  CHECK(CopyJByteArrayToBuf(env, source, buf.get(), &size));
  dest->assign(buf.get(), size);
}

const char *HTTPMethodTypeToChars(mozc::HTTPMethodType type) {
  switch (type) {
    case mozc::HTTP_GET:
      return "GET";
    case mozc::HTTP_HEAD:
      return "HEAD";
    case mozc::HTTP_POST:
      return "POST";
    default:
      LOG(ERROR) << "Invalid method; " << type;
      return NULL;
  }
}

// Utility to enlarge the java's local frame, as RAII idiom, like scoped_ptr.
class ScopedJavaLocalFrame {
 public:
  ScopedJavaLocalFrame(JNIEnv *env, jint capacity) : env_(env) {
    env_->PushLocalFrame(capacity);
  }
  ~ScopedJavaLocalFrame() {
    env_->PopLocalFrame(NULL);
  }

 private:
  JNIEnv *env_;
  DISALLOW_COPY_AND_ASSIGN(ScopedJavaLocalFrame);
};

const jint kDefaultLocalFrameSize = 16;

mozc::Mutex jvm_mutex;

class JavaHttpClientDescriptor {
 public:
  explicit JavaHttpClientDescriptor(JavaVM *jvm) : jvm_(jvm) {
    CHECK(jvm != NULL) << "Given JVM is null.";

    ScopedJavaThreadAttacher thread_attacher(jvm);
    JNIEnv *env = thread_attacher.mutable_jni_env();
    ScopedJavaLocalFrame local_frame(env, kDefaultLocalFrameSize);

    const char kHttpClientClassPath[] =
        "org/mozc/android/inputmethod/japanese/nativecallback/HttpClient";
    jclass http_Client_class = env->FindClass(kHttpClientClassPath);
    CHECK(http_Client_class != NULL)
        << kHttpClientClassPath << " is not found.";
    // We need to keep "global" reference for jclass instance, in order to
    // avoid the garbage collection of the encryptor class.
    http_client_class_ =
        static_cast<jclass>(env->NewGlobalRef(http_Client_class));

    request_id_ = env->GetStaticMethodID(
        http_client_class_, "request", "([B[B[B)[B");
  }

  ~JavaHttpClientDescriptor() {
    ScopedJavaThreadAttacher thread_attacher(jvm_);
    JNIEnv *env = thread_attacher.mutable_jni_env();
    env->DeleteGlobalRef(http_client_class_);
  }

  JavaVM *mutable_jvm() {
    return jvm_;
  }

  jbyteArray Request(JNIEnv *env,
                     jbyteArray method, jbyteArray url, jbyteArray post_data) {
    jbyteArray result = static_cast<jbyteArray>(env->CallStaticObjectMethod(
        http_client_class_, request_id_, method, url, post_data));

    if (env->ExceptionOccurred() != NULL) {
      env->ExceptionDescribe();
      env->ExceptionClear();
      return NULL;
    }

    return result;
  }

  static JavaHttpClientDescriptor *Create(JavaVM *jvm) {
    if (jvm == NULL) {
      return NULL;
    }
    return new JavaHttpClientDescriptor(jvm);
  }

 private:
  JavaVM *jvm_;
  jclass http_client_class_;
  jmethodID request_id_;

  DISALLOW_COPY_AND_ASSIGN(JavaHttpClientDescriptor);
};
scoped_ptr<JavaHttpClientDescriptor> http_client_descriptor;
}  // namespace

namespace mozc {
namespace jni {

// static
bool JavaHttpClientProxy::Request(HTTPMethodType type,
                                  const string &url,
                                  const char *post_data,
                                  size_t post_size,
                                  const HTTPClient::Option &option,
                                  string *output_string) {
  JavaHttpClientDescriptor *descriptor = http_client_descriptor.get();
  if (descriptor == NULL) {
    LOG(ERROR) << "JavaVM is not initialized.";
    return false;
  }

  ScopedJavaThreadAttacher thread_attacher(descriptor->mutable_jvm());
  JNIEnv *env = thread_attacher.mutable_jni_env();
  if (env == NULL) {
    LOG(ERROR) << "Java env is not available.";
    return false;
  }

  ScopedJavaLocalFrame local_frame(env, kDefaultLocalFrameSize);

  const char *method = HTTPMethodTypeToChars(type);
  if (!method) {
    LOG(ERROR) << "HTTP method type is not accepatable.";
    return false;
  }

  jbyteArray java_result = descriptor->Request(
      env,
      BufferToJByteArray(env, method, strlen(method)),
      BufferToJByteArray(env, url.c_str(), url.length()),
      BufferToJByteArray(env, post_data, post_size));

  if (java_result == NULL) {
    LOG(ERROR) << "Method invocation failed.";
    return false;
  }

  if (output_string) {
    AssignJByteArrayToString(env, java_result, output_string);
  }

  return true;
}

// static
void JavaHttpClientProxy::SetJavaVM(JavaVM *jvm) {
  mozc::scoped_lock lock(&jvm_mutex);
  http_client_descriptor.reset(JavaHttpClientDescriptor::Create(jvm));
}

}  // namespace jni
}  // namespace mozc
