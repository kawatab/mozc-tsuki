// Copyright 2010-2018, Google Inc.
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

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "rewriter/calculator/calculator_interface.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/mozctest.h"

namespace mozc {
namespace {

// Runs calculation with |expression| and compares the result and |expect|.
void VerifyCalculation(const CalculatorInterface *calculator,
                       const string &expression,
                       const string &expected) {
  string result;
  EXPECT_TRUE(calculator->CalculateString(expression, &result))
      << expression << "  expected = " << expected;
  const double result_val = atof(result.c_str());
  const double expected_val = atof(expected.c_str());
  const double err = fabs(result_val - expected_val);

  EXPECT_DOUBLE_EQ(expected_val, result_val)
      << "comparison: " << result_val << " vs " << expected_val << std::endl
      << "error: " << err << std::endl
      << "expr = " << expression << std::endl
      << "result = " << result;
}

// Runs calculation and compare results in PRINTED string.
void VerifyCalculationInString(const CalculatorInterface *calculator,
                               const string &expression,
                               const string &expected) {
  string result;
  EXPECT_TRUE(calculator->CalculateString(expression, &result))
      << expression << "  expected = " << expected;
  EXPECT_EQ(expected, result) << "expr = " << expression << std::endl;
}

// Tries to calculate |wrong_key| and returns true if it fails.
void VerifyRejection(const CalculatorInterface *calculator,
                     const string &wrong_key) {
  string result;
  EXPECT_FALSE(calculator->CalculateString(wrong_key, &result))
      << "expression: " << wrong_key << std::endl;
}

}  // namespace

TEST(CalculatorTest, BasicTest) {
  CalculatorInterface *calculator = CalculatorFactory::GetCalculator();

  // These are not expressions
  // apparently
  VerifyRejection(calculator, "test");
  // Expressoin must be ended with equal '='.
  VerifyRejection(calculator, "5+4");
  // Expression must include at least one operator other than parentheses.
  VerifyRejection(calculator, "111=");
  VerifyRejection(calculator, "(5)=");
  // Expression must include at least one number.
  VerifyRejection(calculator, "()=");
  // Expression with both heading and tailing '='s should be rejected.
  VerifyRejection(calculator, "=(0-0)=");

  // Test for each operators
  VerifyCalculation(calculator, "38+2.5=", "40.5");
  VerifyCalculation(calculator, "5.5-21=", "-15.5");
  VerifyCalculation(calculator, "4*2.1=", "8.4");
  VerifyCalculation(calculator, "8/2=", "4");
  VerifyCalculation(calculator, "15・3=", "5");
  VerifyCalculation(calculator, "100%6=", "4");
  VerifyCalculation(calculator, "2^10=", "1024");
  VerifyCalculation(calculator, "4*-2=", "-8");
  VerifyCalculation(calculator, "-10.3+3.5=", "-6.8");
  // Expression can starts with '=' instead of ending with '='.
  VerifyCalculation(calculator, "=-10.3+3.5", "-6.8");

  // Full width cases (some operators may appear as full width character).
  VerifyCalculation(calculator, "１２３４５＋６７８９０＝", "80235");
  VerifyCalculation(calculator, "5−1=", "4");     // − is U+2212
  VerifyCalculation(calculator, "-ー3+5=", "8");  // ー is U+30FC
  VerifyCalculation(calculator, "1．5＊2=", "3");
  VerifyCalculation(calculator, "10／2=", "5");
  VerifyCalculation(calculator, "2＾ー2=", "0.25");
  VerifyCalculation(calculator, "13％3=", "1");
  VerifyCalculation(calculator, "（1+1）*2=", "4");

  // Expressions with more than one operator.
  VerifyCalculation(calculator, "(1+2)-4=", "-1");
  VerifyCalculation(calculator, "5*(2+3)=", "25");
  VerifyCalculation(calculator, "(70-((3+2)*4))%8=", "2");

  // Issue 3082576: 7472.4 - 7465.6 = 6.7999999999993 is not expected.
  VerifyCalculationInString(calculator, "7472.4-7465.6=", "6.8");
}

// Test large number of queries.  Test data is located at
// data/test/calculator/testset.txt.
// In this file, each test case is written in one line in the format
// "expression=answer".  Answer is suppressed if the expression is invalid,
// i.e. it is a false test.
TEST(CalculatorTest, StressTest) {
  const string filename = testing::GetSourceFileOrDie({
      "data", "test", "calculator", "testset.txt"});
  CalculatorInterface *calculator = CalculatorFactory::GetCalculator();

  std::ifstream finput(filename.c_str());
  string line;
  int lineno = 0;
  while (getline(finput, line)) {
    ++lineno;

    // |line| is of format "expression=answer".
    const size_t index_of_equal = line.find('=');
    DCHECK(index_of_equal != string::npos);
    const size_t query_length = index_of_equal + 1;
    const string query(line, 0, query_length);

    // Smoke test.
    // If (OS_ANDROID && x86) the result differs from expectation
    // because of floating point specification so on such environment
    // Following verification is skipped.
    string unused_result;
    calculator->CalculateString(query, &unused_result);
#if !defined(OS_ANDROID) || !defined(__i386__)
    if (line.size() == query_length) {
      // False test
      VerifyRejection(calculator, line);
      continue;
    }
    const string answer(line, query_length);
    VerifyCalculation(calculator, query, answer);
#endif  // !defined(OS_ANDROID) || !defined(__i386__)
  }
  LOG(INFO) << "done " << lineno << " tests from " << filename << std::endl;
}

}  // namespace mozc
