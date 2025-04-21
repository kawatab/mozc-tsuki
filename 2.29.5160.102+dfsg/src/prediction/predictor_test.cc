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

#include "prediction/predictor.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "composer/composer.h"
#include "config/config_handler.h"
#include "converter/converter_mock.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "prediction/predictor_interface.h"
#include "prediction/user_history_predictor.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "session/request_test_util.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc::prediction {
namespace {

using ::mozc::dictionary::MockDictionary;
using ::mozc::dictionary::SuppressionDictionary;
using ::testing::_;
using ::testing::AtMost;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::Unused;

class CheckCandSizeDictionaryPredictor : public PredictorInterface {
 public:
  explicit CheckCandSizeDictionaryPredictor(int expected_cand_size)
      : expected_cand_size_(expected_cand_size),
        predictor_name_("CheckCandSizeDictionaryPredictor") {}

  bool PredictForRequest(const ConversionRequest &request,
                         Segments *segments) const override {
    EXPECT_EQ(request.max_dictionary_prediction_candidates_size(),
              expected_cand_size_);
    return true;
  }

  const std::string &GetPredictorName() const override {
    return predictor_name_;
  }

 private:
  const int expected_cand_size_;
  const std::string predictor_name_;
};

class CheckCandSizeUserHistoryPredictor : public PredictorInterface {
 public:
  CheckCandSizeUserHistoryPredictor(int expected_cand_size,
                                    int expected_cand_size_for_zero_query)
      : expected_cand_size_(expected_cand_size),
        expected_cand_size_for_zero_query_(expected_cand_size_for_zero_query),
        predictor_name_("CheckCandSizeUserHistoryPredictor") {}

  bool PredictForRequest(const ConversionRequest &request,
                         Segments *segments) const override {
    EXPECT_EQ(request.max_user_history_prediction_candidates_size(),
              expected_cand_size_);
    EXPECT_EQ(
        request.max_user_history_prediction_candidates_size_for_zero_query(),
        expected_cand_size_for_zero_query_);
    return true;
  }

  const std::string &GetPredictorName() const override {
    return predictor_name_;
  }

 private:
  const int expected_cand_size_;
  const int expected_cand_size_for_zero_query_;
  const std::string predictor_name_;
};

class NullPredictor : public PredictorInterface {
 public:
  explicit NullPredictor(bool ret)
      : return_value_(ret),
        predict_called_(false),
        predictor_name_("NullPredictor") {}

  bool PredictForRequest(const ConversionRequest &request,
                         Segments *segments) const override {
    predict_called_ = true;
    return return_value_;
  }

  bool predict_called() const { return predict_called_; }

  void Clear() { predict_called_ = false; }

  const std::string &GetPredictorName() const override {
    return predictor_name_;
  }

 private:
  bool return_value_;
  mutable bool predict_called_;
  const std::string predictor_name_;
};

class MockPredictor : public PredictorInterface {
 public:
  MockPredictor() = default;
  ~MockPredictor() override = default;
  MOCK_METHOD(bool, PredictForRequest,
              (const ConversionRequest &request, Segments *segments),
              (const, override));
  MOCK_METHOD(const std::string &, GetPredictorName, (), (const, override));
};

}  // namespace

class MobilePredictorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_ = std::make_unique<config::Config>();
    config::ConfigHandler::GetDefaultConfig(config_.get());

    request_ = std::make_unique<commands::Request>();
    commands::RequestForUnitTest::FillMobileRequest(request_.get());
    composer_ = std::make_unique<composer::Composer>(nullptr, request_.get(),
                                                     config_.get());

    convreq_ = std::make_unique<ConversionRequest>(
        composer_.get(), request_.get(), config_.get());
  }

  std::unique_ptr<mozc::composer::Composer> composer_;
  std::unique_ptr<commands::Request> request_;
  std::unique_ptr<config::Config> config_;
  std::unique_ptr<ConversionRequest> convreq_;
};

TEST_F(MobilePredictorTest, CallPredictorsForMobileSuggestion) {
  MockConverter converter;
  auto predictor = std::make_unique<MobilePredictor>(
      std::make_unique<CheckCandSizeDictionaryPredictor>(20),
      std::make_unique<CheckCandSizeUserHistoryPredictor>(3, 4), &converter);
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::SUGGESTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(MobilePredictorTest, CallPredictorsForMobilePartialSuggestion) {
  MockConverter converter;
  auto predictor = std::make_unique<MobilePredictor>(
      std::make_unique<CheckCandSizeDictionaryPredictor>(20),
      // We don't call history predictior
      std::make_unique<CheckCandSizeUserHistoryPredictor>(-1, -1), &converter);
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::PARTIAL_SUGGESTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(MobilePredictorTest, CallPredictorsForMobilePrediction) {
  MockConverter converter;
  auto predictor = std::make_unique<MobilePredictor>(
      std::make_unique<CheckCandSizeDictionaryPredictor>(200),
      std::make_unique<CheckCandSizeUserHistoryPredictor>(3, 4), &converter);
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::PREDICTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(MobilePredictorTest, CallPredictorsForMobilePartialPrediction) {
  MockConverter converter;
  MockDictionary dictionary_mock;
  testing::MockDataManager data_manager;
  const dictionary::PosMatcher pos_matcher(data_manager.GetPosMatcherData());
  const SuppressionDictionary suppression_dictionary;
  auto predictor = std::make_unique<MobilePredictor>(
      std::make_unique<CheckCandSizeDictionaryPredictor>(200),
      std::make_unique<UserHistoryPredictor>(&dictionary_mock, &pos_matcher,
                                             &suppression_dictionary, true),
      &converter);
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::PARTIAL_PREDICTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(MobilePredictorTest, CallPredictForRequestMobile) {
  auto predictor1 = std::make_unique<MockPredictor>();
  auto predictor2 = std::make_unique<MockPredictor>();
  EXPECT_CALL(*predictor1, PredictForRequest(_, _))
      .Times(AtMost(1))
      .WillOnce(Return(true));
  EXPECT_CALL(*predictor2, PredictForRequest(_, _))
      .Times(AtMost(1))
      .WillOnce(Return(true));

  MockConverter converter;
  auto predictor = std::make_unique<MobilePredictor>(
      std::move(predictor1), std::move(predictor2), &converter);
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::SUGGESTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

class PredictorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_ = std::make_unique<config::Config>();
    config::ConfigHandler::GetDefaultConfig(config_.get());

    request_ = std::make_unique<commands::Request>();
    composer_ = std::make_unique<composer::Composer>(nullptr, request_.get(),
                                                     config_.get());
    convreq_ = std::make_unique<ConversionRequest>(
        composer_.get(), request_.get(), config_.get());
  }

  std::unique_ptr<mozc::composer::Composer> composer_;
  std::unique_ptr<commands::Request> request_;
  std::unique_ptr<config::Config> config_;
  std::unique_ptr<ConversionRequest> convreq_;
};

TEST_F(PredictorTest, AllPredictorsReturnTrue) {
  MockConverter converter;
  auto predictor = std::make_unique<DefaultPredictor>(
      std::make_unique<NullPredictor>(true),
      std::make_unique<NullPredictor>(true), &converter);
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::SUGGESTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(PredictorTest, MixedReturnValue) {
  MockConverter converter;
  auto predictor = std::make_unique<DefaultPredictor>(
      std::make_unique<NullPredictor>(true),
      std::make_unique<NullPredictor>(false), &converter);
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::SUGGESTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(PredictorTest, AllPredictorsReturnFalse) {
  MockConverter converter;
  auto predictor = std::make_unique<DefaultPredictor>(
      std::make_unique<NullPredictor>(false),
      std::make_unique<NullPredictor>(false), &converter);
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::SUGGESTION);
  EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(PredictorTest, CallPredictorsForSuggestion) {
  MockConverter converter;
  const int suggestions_size =
      config::ConfigHandler::DefaultConfig().suggestions_size();
  auto predictor = std::make_unique<DefaultPredictor>(
      std::make_unique<CheckCandSizeDictionaryPredictor>(suggestions_size),
      std::make_unique<CheckCandSizeUserHistoryPredictor>(suggestions_size,
                                                          suggestions_size),
      &converter);
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::SUGGESTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(PredictorTest, CallPredictorsForPrediction) {
  MockConverter converter;
  constexpr int kPredictionSize = 100;
  auto predictor = std::make_unique<DefaultPredictor>(
      std::make_unique<CheckCandSizeDictionaryPredictor>(kPredictionSize),
      std::make_unique<CheckCandSizeUserHistoryPredictor>(kPredictionSize,
                                                          kPredictionSize),
      &converter);
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::PREDICTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(PredictorTest, CallPredictForRequest) {
  auto predictor1 = std::make_unique<MockPredictor>();
  auto predictor2 = std::make_unique<MockPredictor>();
  EXPECT_CALL(*predictor1, PredictForRequest(_, _))
      .Times(AtMost(1))
      .WillOnce(Return(true));
  EXPECT_CALL(*predictor2, PredictForRequest(_, _))
      .Times(AtMost(1))
      .WillOnce(Return(true));

  MockConverter converter;
  auto predictor = std::make_unique<DefaultPredictor>(
      std::move(predictor1), std::move(predictor2), &converter);
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::SUGGESTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(PredictorTest, DisableAllSuggestion) {
  auto predictor1 = std::make_unique<NullPredictor>(true);
  auto predictor2 = std::make_unique<NullPredictor>(true);
  const auto *pred1 = predictor1.get();  // Keep the reference
  const auto *pred2 = predictor2.get();  // Keep the reference
  MockConverter converter;
  auto predictor = std::make_unique<DefaultPredictor>(
      std::move(predictor1), std::move(predictor2), &converter);
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::SUGGESTION);

  config_->set_presentation_mode(true);
  EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_FALSE(pred1->predict_called());
  EXPECT_FALSE(pred2->predict_called());

  config_->set_presentation_mode(false);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_TRUE(pred1->predict_called());
  EXPECT_TRUE(pred2->predict_called());
}

TEST_F(PredictorTest, PopulateReadingOfCommittedCandidateIfMissing) {
  MockConverter converter;
  auto predictor = std::make_unique<MobilePredictor>(
      std::make_unique<NullPredictor>(true),
      std::make_unique<NullPredictor>(true), &converter);

  // Mock reverse conversion adds reading "とうきょう".
  EXPECT_CALL(converter, StartReverseConversion(_, StrEq("東京")))
      .WillRepeatedly([](Segments *segments, Unused) {
        segments->add_segment()->add_candidate()->value = "とうきょう";
        return true;
      });

  // Test the case where value == content_value.
  {
    Segments segments;
    Segment *segment = segments.add_segment();

    Segment::Candidate *cand1 = segment->add_candidate();
    cand1->value = "東京";
    cand1->content_value = "東京";

    Segment::Candidate *cand2 = segment->add_candidate();
    cand2->value = "大阪";
    cand2->content_value = "大阪";

    Segment::Candidate *cand3 = segment->add_candidate();
    cand3->value = "群馬";
    cand3->content_value = "群馬";

    predictor->Finish(*convreq_, &segments);
    EXPECT_EQ(cand1->key, "とうきょう");
    EXPECT_EQ(cand1->content_key, "とうきょう");
    EXPECT_TRUE(cand2->key.empty());
    EXPECT_TRUE(cand2->content_key.empty());
    EXPECT_TRUE(cand3->key.empty());
    EXPECT_TRUE(cand3->content_key.empty());
  }
  // Test the case where value != content_value.
  {
    Segments segments;
    Segment *segment = segments.add_segment();

    Segment::Candidate *cand1 = segment->add_candidate();
    cand1->value = "東京に";
    cand1->content_value = "東京";

    Segment::Candidate *cand2 = segment->add_candidate();
    cand2->value = "大阪に";
    cand2->content_value = "大阪";

    Segment::Candidate *cand3 = segment->add_candidate();
    cand3->value = "群馬に";
    cand3->content_value = "群馬";

    predictor->Finish(*convreq_, &segments);
    EXPECT_EQ(cand1->key, "とうきょうに");
    EXPECT_EQ(cand1->content_key, "とうきょう");
    EXPECT_TRUE(cand2->key.empty());
    EXPECT_TRUE(cand2->content_key.empty());
    EXPECT_TRUE(cand3->key.empty());
    EXPECT_TRUE(cand3->content_key.empty());
  }
  // Test the case where value != content_value and the functional value is not
  // Hiragana. We cannot add the reading in this case.
  {
    Segments segments;
    Segment *segment = segments.add_segment();

    Segment::Candidate *cand1 = segment->add_candidate();
    cand1->value = "東京便";
    cand1->content_value = "東京";

    predictor->Finish(*convreq_, &segments);
    EXPECT_TRUE(cand1->key.empty());
    EXPECT_TRUE(cand1->content_key.empty());
  }
  // Test the case where value != content_value and content_value is empty.
  {
    Segments segments;
    Segment *segment = segments.add_segment();

    Segment::Candidate *cand1 = segment->add_candidate();
    cand1->value = "東京";
    cand1->content_value.clear();

    predictor->Finish(*convreq_, &segments);
    EXPECT_TRUE(cand1->key.empty());
    EXPECT_TRUE(cand1->content_key.empty());
  }
}

}  // namespace mozc::prediction
