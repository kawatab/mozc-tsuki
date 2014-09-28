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

#ifndef MOZC_BASE_ENCRYPTOR_H_
#define MOZC_BASE_ENCRYPTOR_H_

#include <string>
#include "base/port.h"
#include "base/scoped_ptr.h"

namespace mozc {

class Encryptor {
 public:
  // Internal class for representing a key
  class Key {
   public:
    // Make a session key from password and salt.
    // You can also set an initialization vector whose
    // size must be iv_size().
    // if iv is NULL, default iv is used.
    bool DeriveFromPassword(const string &password,
                            const string &salt,
                            const uint8 *iv);

    // use default iv.
    bool DeriveFromPassword(const string &password,
                            const string &salt) {
      return DeriveFromPassword(password, salt, NULL);
    }

    // use empty salt and default iv
    bool DeriveFromPassword(const string &password) {
      return DeriveFromPassword(password, "", NULL);
    }

    // return block size. the result should be 16byte with AES
    size_t block_size() const;

    // return initialization vector
    const uint8* iv() const;

    // return the size of initialization vector
    // the result should be the same as block_size() with AES
    size_t iv_size() const;

    // key length (bit)
    size_t key_size() const;

    // return true if the key is ready
    bool IsAvailable() const;

    // return the size required to encrypt the buffer of size |size|.
    size_t GetEncryptedSize(size_t size) const;

    Key();
    ~Key();

    struct InternalData;
    scoped_ptr<InternalData> data_;
  };

  // Encrypt character buffer. set the size of character buffer
  // in *buf_size. This function stores the size of result buffer in
  // *buf_size. Note that the capacity of buffer MUST BE LARGER
  // than the result of Encrypt::Key::GetEncryptedSize(*buf_size),
  // since this method will add an extra padding to the buffer.
  static bool EncryptArray(const Key &key, char *buf, size_t *buf_size);

  // Decrypt character buffer. set the size of character buffer
  // in *buf_size. This function stores the size of buffer in *buf_size.
  static bool DecryptArray(const Key &key, char *buf, size_t *buf_size);

  // Encrypt string with key.
  static bool EncryptString(const Key &key, string *data);

  // Encrypt string with key.
  static bool DecryptString(const Key &key, string *data);

  // Encrypt string to protect plain_text which may contain
  // sensitive data, like auth_token, password ..etc.
  // It uses CryptProtectData API to encrypt data on Windows.
  // http://msdn.microsoft.com/en-us/library/aa380261.aspx
  // Basically, it uses a OS-specific encrpytor object to
  // encrypt/decrypt data
  static bool ProtectData(const string &plain_text, string *cipher_text);

  // Decrpyt string to unprotect cipher_text.
  // It uses CryptUnprotectData API to decrypt data on Windows.
  static bool UnprotectData(const string &cipher_text, string *plain_text);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Encryptor);
};
}  // namespace mozc
#endif  // MOZC_BASE_ENCRYPTOR_H_
