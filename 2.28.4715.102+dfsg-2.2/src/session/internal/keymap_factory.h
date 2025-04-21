// Copyright 2010-2021, Google Inc.
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

#ifndef MOZC_SESSION_INTERNAL_KEYMAP_FACTORY_H_
#define MOZC_SESSION_INTERNAL_KEYMAP_FACTORY_H_

#include <map>

#include "base/freelist.h"
#include "protocol/config.pb.h"

namespace mozc {
namespace keymap {

class KeyMapManager;

class KeyMapFactory {
 public:
  using KeyMapManagerMap =
      std::map<config::Config::SessionKeymap, KeyMapManager *>;

  KeyMapFactory() = delete;
  ~KeyMapFactory() = delete;

  // Returns KeyMapManager corresponding keymap and custom rule stored in
  // config.  Note, keymap might be different from config.session_keymap.
  static KeyMapManager *GetKeyMapManager(
      const config::Config::SessionKeymap keymap);

  // Reload the custom keymap.
  static void ReloadConfig(const config::Config &config);

 private:
  friend class TestKeyMapFactoryProxy;

  static KeyMapManagerMap *GetKeyMaps();
  static ObjectPool<KeyMapManager> *GetPool();
};

}  // namespace keymap
}  // namespace mozc

#endif  // MOZC_SESSION_INTERNAL_KEYMAP_FACTORY_H_
