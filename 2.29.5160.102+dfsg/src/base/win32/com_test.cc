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

#include "base/win32/com.h"

#include <guiddef.h>
#include <objbase.h>
#include <rpc.h>
#include <shobjidl.h>
#include <unknwn.h>
#include <wil/com.h>
#include <wil/resource.h>

#include <string_view>
#include <utility>

#include "base/win32/com_implements.h"
#include "base/win32/scoped_com.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc::win32 {
namespace {

using ::testing::StrEq;

// Mock interfaces for testing.
MIDL_INTERFACE("A03A80F4-9254-4C8B-AF25-0674FCED18E5")
IMock1 : public IUnknown {
  STDMETHOD(Test1)() = 0;
  STDMETHOD_(LONG, GetQICountAndReset)() = 0;
};

MIDL_INTERFACE("863EF391-8485-4257-8423-8D919D1AE8DC")
IMock2 : public IUnknown { STDMETHOD(Test2)() = 0; };

MIDL_INTERFACE("7CC0C082-8CA5-4A87-97C4-4FC14FBCE0B3")
IDerived : public IMock1 { STDMETHOD(Derived()) = 0; };

}  // namespace

// Define outside the anonymous namespace.
template <>
bool IsIIDOf<IDerived>(REFIID riid) {
  return IsIIDOf<IDerived, IMock1>(riid);
}

namespace {

int object_count;  // Number of Mock instances.

class Mock : public ComImplements<ComImplementsTraits, IMock2, IDerived> {
 public:
  Mock() { ++object_count; }
  ~Mock() override { --object_count; }

  STDMETHODIMP QueryInterface(REFIID iid, void **out) override {
    qi_count_++;
    return ComImplements::QueryInterface(iid, out);
  }
  STDMETHODIMP Test1() override { return S_OK; }
  STDMETHODIMP Test2() override { return S_FALSE; }
  STDMETHODIMP Derived() override { return 2; }
  STDMETHODIMP_(LONG) GetQICountAndReset() override {
    return std::exchange(qi_count_, 0);
  }

 private:
  int qi_count_ = 0;
};

class ComTest : public ::testing::Test {
 protected:
  ComTest() { object_count = 0; }
  ~ComTest() override { EXPECT_EQ(object_count, 0); }

 private:
  ScopedCOMInitializer initializer_;
};

TEST_F(ComTest, ComCreateInstance) {
  wil::com_ptr_nothrow<IShellLink> shellink =
      ComCreateInstance<IShellLink, ShellLink>();
  EXPECT_TRUE(shellink);
  EXPECT_TRUE(ComCreateInstance<IShellLink>(CLSID_ShellLink));
  EXPECT_FALSE(ComCreateInstance<IShellFolder>(CLSID_ShellLink));
}

TEST_F(ComTest, MakeComPtr) {
  auto ptr = MakeComPtr<Mock>();
  EXPECT_TRUE(ptr);
  EXPECT_EQ(object_count, 1);
  EXPECT_EQ(ptr->GetQICountAndReset(), 0);
}

TEST_F(ComTest, ComQuery) {
  wil::com_ptr_nothrow<IMock1> mock1(MakeComPtr<Mock>());
  EXPECT_TRUE(mock1);
  EXPECT_EQ(mock1->Test1(), S_OK);

  wil::com_ptr_nothrow<IDerived> derived = ComQuery<IDerived>(mock1);
  EXPECT_TRUE(derived);
  EXPECT_EQ(derived->Derived(), 2);
  EXPECT_EQ(derived->GetQICountAndReset(), 1);

  EXPECT_TRUE(ComQuery<IMock1>(derived));
  EXPECT_EQ(derived->GetQICountAndReset(), 0);

  wil::com_ptr_nothrow<IMock2> mock2 = ComQuery<IMock2>(mock1);
  EXPECT_TRUE(mock2);
  EXPECT_EQ(mock2->Test2(), S_FALSE);
  EXPECT_EQ(mock1->GetQICountAndReset(), 1);

  mock2 = ComQuery<IMock2>(mock1);
  EXPECT_TRUE(mock2);
  EXPECT_EQ(mock2->Test2(), S_FALSE);
  EXPECT_EQ(mock1->GetQICountAndReset(), 1);

  EXPECT_EQ(ComQueryHR<IShellView>(mock2).hr(), E_NOINTERFACE);
  EXPECT_EQ(mock1->GetQICountAndReset(), 1);
}

TEST_F(ComTest, ComCopy) {
  wil::com_ptr_nothrow<IMock1> mock1(MakeComPtr<Mock>());
  EXPECT_TRUE(mock1);
  EXPECT_EQ(mock1->Test1(), S_OK);

  wil::com_ptr_nothrow<IUnknown> unknown = ComCopy<IUnknown>(mock1);
  EXPECT_TRUE(unknown);
  EXPECT_EQ(mock1->GetQICountAndReset(), 0);

  EXPECT_FALSE(ComCopy<IShellLink>(unknown));
  EXPECT_EQ(mock1->GetQICountAndReset(), 1);

  IUnknown *null = nullptr;
  EXPECT_FALSE(ComCopy<IUnknown>(null));
}

TEST(ComBSTRTest, MakeUniqueBSTR) {
  EXPECT_FALSE(MakeUniqueBSTR(nullptr).is_valid());
  wil::unique_bstr empty_string = MakeUniqueBSTR(L"");
  EXPECT_THAT(empty_string.get(), StrEq(L""));
  constexpr std::wstring_view kSource = L"こんにちは, Mozc.";
  wil::unique_bstr result = MakeUniqueBSTR(kSource);
  EXPECT_EQ(result.get(), kSource);
}

}  // namespace
}  // namespace mozc::win32
