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

#include "composer/internal/composition_input.h"

#include "testing/base/public/gunit.h"

namespace mozc {

using ProbableKeyEvent = commands::KeyEvent::ProbableKeyEvent;
using ProbableKeyEvents = protobuf::RepeatedPtrField<ProbableKeyEvent>;

namespace composer {

TEST(CompositionInputTest, BasicTest) {
  CompositionInput input;

  {  // Initial status.
    EXPECT_TRUE(input.Empty());
    EXPECT_TRUE(input.raw().empty());
    EXPECT_FALSE(input.has_conversion());
    EXPECT_TRUE(input.conversion().empty());
    EXPECT_TRUE(input.probable_key_events().empty());
    EXPECT_FALSE(input.is_new_input());
  }

  {  // Value setting
    input.set_raw("raw");
    input.set_conversion("conversion");

    ProbableKeyEvents key_events;

    ProbableKeyEvent key_event1;
    key_event1.set_key_code('i');
    key_event1.set_probability(0.6);
    key_events.Add(std::move(key_event1));

    ProbableKeyEvent key_event2;
    key_event2.set_key_code('o');
    key_event2.set_probability(0.4);
    key_events.Add(std::move(key_event2));

    input.set_probable_key_events(key_events);
    input.set_is_new_input(true);

    EXPECT_FALSE(input.Empty());
    EXPECT_EQ("raw", input.raw());
    EXPECT_TRUE(input.has_conversion());
    EXPECT_EQ("conversion", input.conversion());
    EXPECT_EQ(2, input.probable_key_events().size());
    EXPECT_TRUE(input.is_new_input());
  }

  CompositionInput input2;
  {  // Copy and Clear
    input2 = input;
    input.Clear();
    EXPECT_TRUE(input.Empty());
    EXPECT_TRUE(input.raw().empty());
    EXPECT_FALSE(input.has_conversion());
    EXPECT_TRUE(input.conversion().empty());
    EXPECT_TRUE(input.probable_key_events().empty());
    EXPECT_FALSE(input.is_new_input());

    EXPECT_FALSE(input2.Empty());
    EXPECT_EQ("raw", input2.raw());
    EXPECT_TRUE(input2.has_conversion());
    EXPECT_EQ("conversion", input2.conversion());
    EXPECT_EQ(2, input2.probable_key_events().size());
    EXPECT_TRUE(input2.is_new_input());
  }

  {  // Empty conversion string is also a value value.
    input2.set_conversion("");
    EXPECT_TRUE(input2.conversion().empty());
    EXPECT_TRUE(input2.has_conversion());
  }

  {  // Mutable conversion
    ASSERT_TRUE(input.Empty());
    EXPECT_FALSE(input.has_conversion());
    EXPECT_TRUE(input.mutable_conversion()->empty());
    EXPECT_TRUE(input.has_conversion());
    input.mutable_conversion()->assign("mutable_conversion");
    EXPECT_EQ("mutable_conversion", input.conversion());

    ASSERT_FALSE(input2.Empty());
    EXPECT_TRUE(input2.has_conversion());
  }
}

}  // namespace composer
}  // namespace mozc
