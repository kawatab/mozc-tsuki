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

#ifndef MOZC_REWRITER_NUMBER_REWRITER_H_
#define MOZC_REWRITER_NUMBER_REWRITER_H_

#include "base/port.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

class ConversionRequest;
class DataManagerInterface;
class POSMatcher;
class Segments;
struct CounterSuffixEntry;

class NumberRewriter : public RewriterInterface  {
 public:
  explicit NumberRewriter(const DataManagerInterface *data_manager);
  virtual ~NumberRewriter();

  virtual int capability(const ConversionRequest &request) const;

  virtual bool Rewrite(const ConversionRequest &request,
                       Segments *segments) const;

 private:
  const CounterSuffixEntry *suffix_array_;
  size_t suffix_array_size_;
  const POSMatcher *pos_matcher_;

  DISALLOW_COPY_AND_ASSIGN(NumberRewriter);
};

}  // namespace mozc

#endif  // MOZC_REWRITER_NUMBER_REWRITER_H_
