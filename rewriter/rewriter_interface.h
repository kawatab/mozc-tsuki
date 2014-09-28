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

#ifndef MOZC_REWRITER_REWRITER_INTERFACE_H_
#define MOZC_REWRITER_REWRITER_INTERFACE_H_

#include <cstddef>  // for size_t

namespace mozc {

class ConversionRequest;
class Segments;

class RewriterInterface {
 public:
  virtual ~RewriterInterface() {}

  enum CapabilityType {
    NOT_AVAILABLE = 0,
    CONVERSION = 1,
    PREDICTION = 2,
    SUGGESTION = 4,
    ALL = (1 | 2 | 4),
  };

  // return capablity of this rewriter.
  // If (capability() & CONVERSION), this rewriter
  // is called after StartConversion().
  virtual int capability(const ConversionRequest &request) const {
    return CONVERSION;
  }

  virtual bool Rewrite(const ConversionRequest &request,
                       Segments *segments) const = 0;

  // This method is mainly called when user puts SPACE key
  // and changes the focused candidate.
  // In this method, Converter will find bracketing matching.
  // e.g., when user selects "「",  corresponding closing bracket "」"
  // is chosen in the preedit.
  virtual bool Focus(Segments *segments,
                     size_t segment_index,
                     int candidate_index) const {
    return true;
  }

  // Hook(s) for all mutable operations
  virtual void Finish(const ConversionRequest &request, Segments *segments) {}

  // sync internal data to local file system.
  virtual bool Sync() { return true; }

  // reload internal data from local file system.
  virtual bool Reload() { return true; }

  // clear internal data
  virtual void Clear() {}

 protected:
  RewriterInterface() {}
};


}  // namespace mozc

#endif  // MOZC_REWRITER_REWRITER_INTERFACE_H_
