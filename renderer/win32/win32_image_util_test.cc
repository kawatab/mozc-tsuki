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

#include "renderer/win32/win32_image_util.h"

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlapp.h>
#include <atlgdi.h>
#include <atlmisc.h>
#include <gdiplus.h>

#include <fstream>
#include <list>
#include <memory>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "base/util.h"
#include "base/win_font_test_helper.h"
#include "net/jsoncpp.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_srcdir);

namespace mozc {
namespace renderer {
namespace win32 {
namespace {

using ::mozc::renderer::win32::internal::GaussianBlur;
using ::mozc::renderer::win32::internal::SafeFrameBuffer;
using ::mozc::renderer::win32::internal::SubdivisionalPixel;
using ::mozc::renderer::win32::internal::TextLabel;
using ::std::unique_ptr;

using ::WTL::CBitmap;
using ::WTL::CDC;
using ::WTL::CLogFont;
using ::WTL::CPoint;
using ::WTL::CSize;

typedef SubdivisionalPixel::SubdivisionalPixelIterator
    SubdivisionalPixelIterator;

class BalloonImageTest : public testing::Test,
                         public testing::WithParamInterface<const char *> {
 public:
  static void SetUpTestCase() {
    InitGdiplus();

    // On Windows XP, the availability of typical Japanese fonts such are as
    // MS Gothic depends on the language edition and language packs.
    // So we will register a private font for unit test.
    EXPECT_TRUE(WinFontTestHelper::Initialize());
  }

  static void TearDownTestCase() {
    // Free private fonts although the system automatically frees them when
    // this process is terminated.
    WinFontTestHelper::Uninitialize();

    UninitGdiplus();
  }

 protected:
  class TestableBalloonImage : public BalloonImage {
   public:
    using BalloonImage::CreateInternal;
  };

  static void BalloonImageTest::SaveTestImage(
      const TestableBalloonImage::BalloonImageInfo &info,
      const wstring filename) {
    CPoint tail_offset;
    CSize size;
    vector<ARGBColor> buffer;
    CBitmap dib = TestableBalloonImage::CreateInternal(
        info, &tail_offset, &size, &buffer);

    Json::Value tail;
    BalloonInfoToJson(info, &tail);
    tail["output"]["tail_offset_x"] = tail_offset.x;
    tail["output"]["tail_offset_y"] = tail_offset.y;

    Gdiplus::Bitmap bitmap(size.cx, size.cy);
    for (size_t y = 0; y < size.cy; ++y) {
      for (size_t x = 0; x < size.cx; ++x) {
        ARGBColor argb = buffer[y * size.cx + x];
        const Gdiplus::Color color(argb.a, argb.r, argb.g, argb.b);
        bitmap.SetPixel(x, y, color);
      }
    }

    bitmap.Save(filename.c_str(), &clsid_png_);

    string utf8_filename;
    Util::WideToUTF8(filename + L".json", &utf8_filename);
    OutputFileStream os(utf8_filename.c_str());
    Json::StyledWriter writer;
    os << writer.write(tail);
  }

  static void BalloonInfoToJson(
      const TestableBalloonImage::BalloonImageInfo &info, Json::Value *tail) {
    Json::Value input(Json::objectValue);
    input["frame_color"] = Json::Value(ColorToInteger(info.frame_color));
    input["inside_color"] = Json::Value(ColorToInteger(info.inside_color));
    input["label_color"] = Json::Value(ColorToInteger(info.label_color));
    input["blur_color"] = Json::Value(ColorToInteger(info.blur_color));
    input["blur_alpha"] = Json::Value(info.blur_alpha);
    input["label_size"] = Json::Value(info.label_size);
    input["label_font"] = Json::Value(info.label_font);
    input["label"] = Json::Value(info.label);
    input["rect_width"] = Json::Value(info.rect_width);
    input["rect_height"] = Json::Value(info.rect_height);
    input["frame_thickness"] = Json::Value(info.frame_thickness);
    input["corner_radius"] = Json::Value(info.corner_radius);
    input["tail_height"] = Json::Value(info.tail_height);
    input["tail_width"] = Json::Value(info.tail_width);
    input["tail_direction"] =
        Json::Value(static_cast<int>(info.tail_direction));
    input["blur_sigma"] = Json::Value(info.blur_sigma);
    input["blur_offset_x"] = Json::Value(info.blur_offset_x);
    input["blur_offset_y"] = Json::Value(info.blur_offset_y);

    (*tail)["input"] = input;
  }

  static void JsonToBalloonInfo(
      const Json::Value &tail, TestableBalloonImage::BalloonImageInfo *info) {
    const Json::Value &input = tail["input"];

    *info = TestableBalloonImage::BalloonImageInfo();
    info->frame_color = IntegerToColor(input["frame_color"].asInt());
    info->inside_color = IntegerToColor(input["inside_color"].asInt());
    info->label_color = IntegerToColor(input["label_color"].asInt());
    info->blur_color = IntegerToColor(input["blur_color"].asInt());
    info->blur_alpha = input["blur_alpha"].asDouble();
    info->label_size = input["label_size"].asInt();
    info->label_font = input["label_font"].asString();
    info->label = input["label"].asString();
    info->rect_width = input["rect_width"].asDouble();
    info->rect_height = input["rect_height"].asDouble();
    info->frame_thickness = input["frame_thickness"].asDouble();
    info->corner_radius = input["corner_radius"].asDouble();
    info->tail_height = input["tail_height"].asDouble();
    info->tail_width = input["tail_width"].asDouble();
    info->tail_direction =
        static_cast<TestableBalloonImage::BalloonImageInfo::TailDirection>(
            input["tail_direction"].asInt());
    info->blur_sigma = input["blur_sigma"].asDouble();
    info->blur_offset_x = input["blur_offset_x"].asInt();
    info->blur_offset_y = input["blur_offset_y"].asInt();
  }

 private:
  static bool GetEncoderClsid(const wstring format, CLSID *clsid) {
    UINT num_codecs = 0;
    UINT codecs_buffer_size = 0;
    Gdiplus::GetImageEncodersSize(&num_codecs, &codecs_buffer_size);
    if (codecs_buffer_size == 0) {
      return false;
    }

    unique_ptr<uint8[]> codesc_buffer(new uint8[codecs_buffer_size]);
    Gdiplus::ImageCodecInfo *codecs =
        reinterpret_cast<Gdiplus::ImageCodecInfo *>(codesc_buffer.get());

    Gdiplus::GetImageEncoders(num_codecs, codecs_buffer_size, codecs);
    for (size_t i = 0; i < num_codecs; ++i) {
      const Gdiplus::ImageCodecInfo &info = codecs[i];
      if (format == info.MimeType) {
        *clsid = info.Clsid;
        return true;
      }
    }
    return false;
  }

  static void InitGdiplus() {
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartup(&gdiplus_token_, &input, nullptr);

    if (!GetEncoderClsid(L"image/png", &clsid_png_)) {
      clsid_png_ = CLSID_NULL;
    }
    if (!GetEncoderClsid(L"image/bmp", &clsid_bmp_)) {
      clsid_bmp_ = CLSID_NULL;
    }
  }

  static void UninitGdiplus() {
    Gdiplus::GdiplusShutdown(gdiplus_token_);
  }

  static int32 ColorToInteger(RGBColor color) {
    return static_cast<int32>(color.r) << 16 |
           static_cast<int32>(color.g) << 8 |
           static_cast<int32>(color.b);
  }

  static RGBColor IntegerToColor(int32 color) {
    return RGBColor((color >> 16) & 0xff,
                    (color >> 8) & 0xff,
                    color & 0xff);
  }

  static CLSID clsid_png_;
  static CLSID clsid_bmp_;

  static HANDLE font_handle_;
  static ULONG_PTR gdiplus_token_;
};

HANDLE BalloonImageTest::font_handle_;
CLSID BalloonImageTest::clsid_png_;
CLSID BalloonImageTest::clsid_bmp_;
ULONG_PTR BalloonImageTest::gdiplus_token_;

// Tests should be passed.
const char *kRenderingResultList[] = {
  "data/test/renderer/win32/balloon_blur_alpha_-1.png",
  "data/test/renderer/win32/balloon_blur_alpha_0.png",
  "data/test/renderer/win32/balloon_blur_alpha_10.png",
  "data/test/renderer/win32/balloon_blur_color_32_64_128.png",
  "data/test/renderer/win32/balloon_blur_offset_-20_-10.png",
  "data/test/renderer/win32/balloon_blur_offset_0_0.png",
  "data/test/renderer/win32/balloon_blur_offset_20_5.png",
  "data/test/renderer/win32/balloon_blur_sigma_0.0.png",
  "data/test/renderer/win32/balloon_blur_sigma_0.5.png",
  "data/test/renderer/win32/balloon_blur_sigma_1.0.png",
  "data/test/renderer/win32/balloon_blur_sigma_2.0.png",
  "data/test/renderer/win32/balloon_frame_thickness_-1.png",
  "data/test/renderer/win32/balloon_frame_thickness_0.png",
  "data/test/renderer/win32/balloon_frame_thickness_1.5.png",
  "data/test/renderer/win32/balloon_frame_thickness_3.png",
  "data/test/renderer/win32/balloon_inside_color_32_64_128.png",
  "data/test/renderer/win32/balloon_no_label.png",
  "data/test/renderer/win32/balloon_tail_bottom.png",
  "data/test/renderer/win32/balloon_tail_left.png",
  "data/test/renderer/win32/balloon_tail_right.png",
  "data/test/renderer/win32/balloon_tail_top.png",
  "data/test/renderer/win32/balloon_tail_width_height_-10_-10.png",
  "data/test/renderer/win32/balloon_tail_width_height_0_0.png",
  "data/test/renderer/win32/balloon_tail_width_height_10_20.png",
  "data/test/renderer/win32/balloon_width_height_40_30.png",
};

INSTANTIATE_TEST_CASE_P(BalloonImageParameters,
                        BalloonImageTest,
                        ::testing::ValuesIn(kRenderingResultList));

TEST_P(BalloonImageTest, TestImpl) {
  string expected_image = GetParam();
  const string &expected_image_path =
      FileUtil::JoinPath(FLAGS_test_srcdir, expected_image);
  ASSERT_TRUE(FileUtil::FileExists(expected_image_path))
      << "Reference file is not found: " << expected_image_path;
  const string json_path = expected_image_path + ".json";
  ASSERT_TRUE(FileUtil::FileExists(json_path))
      << "Manifest file is not found: " << json_path;

  Json::Value tail;
  {
    InputFileStream fs(json_path.c_str());
    ASSERT_TRUE(fs.good());
    fs >> tail;
  }
  TestableBalloonImage::BalloonImageInfo info;
  JsonToBalloonInfo(tail, &info);

  CPoint actual_tail_offset;
  CSize actual_size;
  vector<ARGBColor> actual_buffer;
  CBitmap dib = TestableBalloonImage::CreateInternal(
      info, &actual_tail_offset, &actual_size, &actual_buffer);

  EXPECT_EQ(tail["output"]["tail_offset_x"].asInt(), actual_tail_offset.x);
  EXPECT_EQ(tail["output"]["tail_offset_y"].asInt(), actual_tail_offset.y);

  wstring wide_path;
  Util::UTF8ToWide(expected_image_path, &wide_path);

  Gdiplus::Bitmap bitmap(wide_path.c_str());

  ASSERT_EQ(bitmap.GetWidth(), actual_size.cx);
  ASSERT_EQ(bitmap.GetHeight(), actual_size.cy);

  for (size_t y = 0; y < actual_size.cy; ++y) {
    for (size_t x = 0; x < actual_size.cx; ++x) {
      ARGBColor argb = actual_buffer[y * actual_size.cx + x];
      Gdiplus::Color color;
      ASSERT_EQ(Gdiplus::Ok, bitmap.GetPixel(x, y, &color));
      EXPECT_EQ(color.GetA(), argb.a) << "(x, y): (" << x << ", " << y << ")";
      EXPECT_EQ(color.GetR(), argb.r) << "(x, y): (" << x << ", " << y << ")";
      EXPECT_EQ(color.GetG(), argb.g) << "(x, y): (" << x << ", " << y << ")";
      EXPECT_EQ(color.GetB(), argb.b) << "(x, y): (" << x << ", " << y << ")";
    }
  }
}

TEST(RGBColorTest, BasicTest) {
  EXPECT_NE(RGBColor::kBlack, RGBColor::kWhite);
  EXPECT_EQ(RGBColor::kWhite, RGBColor::kWhite);
}

TEST(ARGBColorTest, BasicTest) {
  EXPECT_NE(ARGBColor::kBlack, ARGBColor::kWhite);
  EXPECT_EQ(ARGBColor::kWhite, ARGBColor::kWhite);
}

TEST(SubdivisionalPixelTest, BasicTest) {
  const RGBColor kBlue(0, 0, 255);
  const RGBColor kGreen(0, 255, 0);

  SubdivisionalPixel sub_pixel;
  EXPECT_EQ(0.0, sub_pixel.GetCoverage())
      << "Should be zero for an empty pixel";
  EXPECT_EQ(RGBColor::kBlack, sub_pixel.GetPixelColor())
      << "Should be black for an empty pixel";

  // SetSubdivisionalPixel sets only sub-pixel specified.
  sub_pixel.SetSubdivisionalPixel(SubdivisionalPixel::Fraction2D(0, 0),
                                  RGBColor::kWhite);
  EXPECT_NEAR(1.0 / 255.0, sub_pixel.GetCoverage(), 0.1);
  EXPECT_EQ(RGBColor::kWhite, sub_pixel.GetPixelColor());

  sub_pixel.SetColorToFilledPixels(kGreen);
  EXPECT_NEAR(1.0 / 255.0, sub_pixel.GetCoverage(), 0.1);
  EXPECT_EQ(kGreen, sub_pixel.GetPixelColor());

  // SetPixel sets all the sub-pixels.
  sub_pixel.SetPixel(kBlue);
  EXPECT_NEAR(1.0, sub_pixel.GetCoverage(), 0.01);
  EXPECT_EQ(kBlue, sub_pixel.GetPixelColor());

  sub_pixel.SetSubdivisionalPixel(SubdivisionalPixel::Fraction2D(0, 0),
                                  RGBColor::kWhite);
  EXPECT_NEAR(1.0, sub_pixel.GetCoverage(), 0.01);
  EXPECT_EQ(1, sub_pixel.GetPixelColor().r);

  sub_pixel.SetColorToFilledPixels(kBlue);
  EXPECT_NEAR(1.0, sub_pixel.GetCoverage(), 0.01);
  EXPECT_EQ(kBlue, sub_pixel.GetPixelColor());
}

TEST(SubdivisionalPixelTest, IteratorTest) {
  const RGBColor kBlue(0, 0, 255);

  SubdivisionalPixel sub_pixel;
  size_t count = 0;
  for (SubdivisionalPixelIterator it(0, 0); !it.Done(); it.Next()) {
    EXPECT_LE(0, it.GetFraction().x);
    EXPECT_LE(0, it.GetFraction().y);
    EXPECT_GT(SubdivisionalPixel::kDivision, it.GetFraction().x);
    EXPECT_GT(SubdivisionalPixel::kDivision, it.GetFraction().y);
    EXPECT_LE(0.0, it.GetX());
    EXPECT_LE(0.0, it.GetY());
    EXPECT_GE(1.0, it.GetX());
    EXPECT_GE(1.0, it.GetY());
    ++count;
  }
  EXPECT_EQ(SubdivisionalPixel::kTotalPixels, count);
}

TEST(GaussianBlurTest, NoBlurTest) {
  // When Gaussian blur sigma is 0.0, no blur effect should be applied.
  GaussianBlur blur(0.0);

  struct Source {
    Source()
        : call_count_(0) {}
    double operator()(int x, int y) const {
      EXPECT_EQ(0, x);
      EXPECT_EQ(0, y);
      ++call_count_;
      return 1.0;
    }
    mutable int call_count_;
  };

  Source source;
  EXPECT_EQ(1.0, blur.Apply(0, 0, source));
  EXPECT_EQ(1, source.call_count_);
}

TEST(GaussianBlurTest, InvalidParamBlurTest) {
  // When Gaussian blur sigma is invalid (a negative value), no blur effect
  // should be applied.

  GaussianBlur blur(-100.0);
  struct Source {
    Source()
        : call_count_(0) {}
    double operator()(int x, int y) const {
      EXPECT_EQ(0, x);
      EXPECT_EQ(0, y);
      ++call_count_;
      return 1.0;
    }
    mutable int call_count_;
  };

  Source source;
  EXPECT_EQ(1.0, blur.Apply(0, 0, source));
  EXPECT_EQ(1, source.call_count_);
}

TEST(GaussianBlurTest, NormalBlurTest) {
  GaussianBlur blur(1.0);
  struct Source {
    explicit Source(int cutoff_length)
        : call_count_(0),
          cutoff_length_(cutoff_length) {}
    double operator()(int x, int y) const {
      EXPECT_GE(cutoff_length_, abs(x));
      EXPECT_GE(cutoff_length_, abs(y));
      ++call_count_;
      return 1.0;
    }
    mutable size_t call_count_;
    int cutoff_length_;
  };

  Source source(blur.cutoff_length());
  EXPECT_NEAR(1.0, blur.Apply(0, 0, source), 0.1);
  const size_t matrix_length = blur.cutoff_length() * 2 + 1;
  EXPECT_EQ(matrix_length * matrix_length, source.call_count_);
}

TEST(SafeFrameBufferTest, BasicTest) {
  const ARGBColor kTransparent(0, 0, 0, 0);
  const int kLeft = -10;
  const int kTop = -20;
  const int kWidth = 50;
  const int kHeight = 100;
  SafeFrameBuffer buffer(Rect(kLeft, kTop, kWidth, kHeight));

  EXPECT_EQ(kTransparent, buffer.GetPixel(kLeft, kTop))
      << "Initial color should be transparent";
  buffer.SetPixel(kLeft, kTop, ARGBColor::kWhite);
  EXPECT_EQ(ARGBColor::kWhite, buffer.GetPixel(kLeft, kTop));

  buffer.SetPixel(kLeft + kWidth, kTop, ARGBColor::kWhite);
  EXPECT_EQ(kTransparent, buffer.GetPixel(kLeft + kWidth, kTop))
      << "(left + width) is outside.";

  buffer.SetPixel(kLeft, kTop + kHeight, ARGBColor::kWhite);
  EXPECT_EQ(kTransparent, buffer.GetPixel(kLeft, kTop + kHeight))
      << "(top + height) is outside.";

  buffer.SetPixel(kLeft - 10, kTop - 10, ARGBColor::kWhite);
  EXPECT_EQ(kTransparent, buffer.GetPixel(kLeft - 10, kTop - 10))
      << "Outside pixel should be kept as transparent.";
}

TEST(TextLabelTest, BoundingBoxTest) {
  const TextLabel label(
      -10.5, -5.1, 10.5, 5.0, "text", "font name", 10, RGBColor::kWhite);
  EXPECT_EQ(-11, label.bounding_rect().Left());
  EXPECT_EQ(-6, label.bounding_rect().Top());
  EXPECT_EQ(0, label.bounding_rect().Right());
  EXPECT_EQ(0, label.bounding_rect().Bottom());
}

}  // namespace
}  // namespace win32
}  // namespace renderer
}  // namespace mozc
