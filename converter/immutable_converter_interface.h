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

#ifndef MOZC_CONVERTER_IMMUTABLE_CONVERTER_INTERFACE_H_
#define MOZC_CONVERTER_IMMUTABLE_CONVERTER_INTERFACE_H_

namespace mozc {

class ConversionRequest;
class Segments;

// Perform one-shot conversion with constraints.
// constraits are encoded in |segments|
class ImmutableConverterInterface {
 public:
  virtual ~ImmutableConverterInterface() {}

  // This method is equivalent to:
  //   const ConversionRequest request;
  //   ConvertForRequest(request, segments);
  // TODO(hidehiko): Deprecate this method and use ConvertForRequest.
  virtual bool Convert(Segments *segments) const;

  // This method should be pure-virtual method in theory.
  // However, to keep the backward compatibility untill the deprecation of
  // Conversion method, we provide the default implementation.
  // Please see the .cc file.
  virtual bool ConvertForRequest(
      const ConversionRequest &request, Segments *segments) const;

 protected:
  ImmutableConverterInterface() {}
};

class ImmutableConverterFactory {
 public:
  // return singleton object
  static ImmutableConverterInterface *GetImmutableConverter();

  // dependency injection for unittesting
  static void SetImmutableConverter(ImmutableConverterInterface
                                    *immutable_converter);

 private:
  ImmutableConverterFactory() {}
  ~ImmutableConverterFactory() {}
};
}  // namespace mozc

#endif  // MOZC_CONVERTER_IMMUTABLE_CONVERTER_INTERFACE_H_
