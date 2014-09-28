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

#ifndef MOZC_REWRITER_ENGLISH_VARIANTS_REWRITER_H_
#define MOZC_REWRITER_ENGLISH_VARIANTS_REWRITER_H_

#include <string>
#include <vector>
#include "converter/segments.h"
#include "rewriter/rewriter_interface.h"
#include "testing/base/public/gunit_prod.h"
// for FRIEND_TEST()

namespace mozc {
class ConversionRequest;

class EnglishVariantsRewriter : public RewriterInterface  {
 public:
  EnglishVariantsRewriter();
  virtual ~EnglishVariantsRewriter();

  virtual int capability(const ConversionRequest &request) const;

  virtual bool Rewrite(const ConversionRequest &request,
                       Segments *segments) const;

 private:
  FRIEND_TEST(EnglishVariantsRewriterTest, ExpandEnglishVariants);
  bool IsT13NCandidate(Segment::Candidate *candidate) const;
  bool IsEnglishCandidate(Segment::Candidate *candidate) const;
  bool ExpandEnglishVariants(const string &input,
                             vector<string> *variants) const;
  bool ExpandEnglishVariantsWithSegment(Segment *seg) const;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_ENGLISH_VARIANTS_REWRITER_H_
