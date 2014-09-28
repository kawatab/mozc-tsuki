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

#include "renderer/renderer_style_handler.h"

#if defined(OS_WIN)
#include <windows.h>
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlapp.h>
#include <atlgdi.h>
#endif  // OS_WIN

#include "base/singleton.h"
#include "renderer/renderer_style.pb.h"

namespace mozc {
namespace renderer {
namespace {
#if defined(OS_WIN)
const int kDefaultDPI = 96;
#endif  // OS_WIN

class RendererStyleHandlerImpl {
 public:
  RendererStyleHandlerImpl();
  ~RendererStyleHandlerImpl() {}
  bool GetRendererStyle(RendererStyle *style);
  bool SetRendererStyle(const RendererStyle &style);
  void GetDefaultRendererStyle(RendererStyle *style);
 private:
  RendererStyle style_;
};

RendererStyleHandlerImpl *GetRendererStyleHandlerImpl() {
  return Singleton<RendererStyleHandlerImpl>::get();
}

RendererStyleHandlerImpl::RendererStyleHandlerImpl() {
  RendererStyle style;
  GetDefaultRendererStyle(&style);
  SetRendererStyle(style);
}

bool RendererStyleHandlerImpl::GetRendererStyle(RendererStyle *style) {
  style->CopyFrom(this->style_);
  return true;
}
bool RendererStyleHandlerImpl::SetRendererStyle(const RendererStyle &style) {
  style_.CopyFrom(style);
  return true;
}
void RendererStyleHandlerImpl::GetDefaultRendererStyle(
    RendererStyle *style) {
  double scale_factor_x = 1.0;
  double scale_factor_y = 1.0;
  RendererStyleHandler::GetDPIScalingFactor(&scale_factor_x,
                                            &scale_factor_y);

  // TODO(horo):Change to read from human-readable ASCII format protobuf.
  style->Clear();
  style->set_window_border(1);  // non-scalable
  style->set_scrollbar_width(4 * scale_factor_x);
  style->set_row_rect_padding(0 * scale_factor_x);
  style->mutable_border_color()->set_r(0x96);
  style->mutable_border_color()->set_g(0x96);
  style->mutable_border_color()->set_b(0x96);

  RendererStyle::TextStyle *shortcutStyle = style->add_text_styles();
  shortcutStyle->set_font_size(14 * scale_factor_y);
  shortcutStyle->mutable_foreground_color()->set_r(0x77);
  shortcutStyle->mutable_foreground_color()->set_g(0x77);
  shortcutStyle->mutable_foreground_color()->set_b(0x77);
  shortcutStyle->mutable_background_color()->set_r(0xf3);
  shortcutStyle->mutable_background_color()->set_g(0xf4);
  shortcutStyle->mutable_background_color()->set_b(0xff);
  shortcutStyle->set_left_padding(8 * scale_factor_x);
  shortcutStyle->set_right_padding(8 * scale_factor_x);

  RendererStyle::TextStyle *gap1Style = style->add_text_styles();
  gap1Style->set_font_size(14 * scale_factor_y);

  RendererStyle::TextStyle *candidateStyle = style->add_text_styles();
  candidateStyle->set_font_size(14 * scale_factor_y);

  RendererStyle::TextStyle *descriptionStyle = style->add_text_styles();
  descriptionStyle->set_font_size(12 * scale_factor_y);
  descriptionStyle->mutable_foreground_color()->set_r(0x88);
  descriptionStyle->mutable_foreground_color()->set_g(0x88);
  descriptionStyle->mutable_foreground_color()->set_b(0x88);
  descriptionStyle->set_right_padding(8 * scale_factor_x);

  // We want to ensure that the candidate window is at least wide
  // enough to render "そのほかの文字種  " as a candidate.
  style->set_column_minimum_width_string(
    "\xE3\x81\x9D\xE3\x81\xAE\xE3\x81\xBB\xE3\x81\x8B\xE3\x81\xAE"
    "\xE6\x96\x87\xE5\xAD\x97\xE7\xA8\xAE  ");

  style->mutable_footer_style()->set_font_size(14 * scale_factor_y);
  style->mutable_footer_style()->set_left_padding(4 * scale_factor_x);
  style->mutable_footer_style()->set_right_padding(4 * scale_factor_x);

  RendererStyle::TextStyle *footer_sub_label_style =
    style->mutable_footer_sub_label_style();
  footer_sub_label_style->set_font_size(10 * scale_factor_y);
  footer_sub_label_style->mutable_foreground_color()->set_r(167);
  footer_sub_label_style->mutable_foreground_color()->set_g(167);
  footer_sub_label_style->mutable_foreground_color()->set_b(167);
  footer_sub_label_style->set_left_padding(4 * scale_factor_x);
  footer_sub_label_style->set_right_padding(4 * scale_factor_x);

  RendererStyle::RGBAColor *color = style->add_footer_border_colors();
  color->set_r(96);
  color->set_r(96);
  color->set_r(96);

  style->mutable_footer_top_color()->set_r(0xff);
  style->mutable_footer_top_color()->set_g(0xff);
  style->mutable_footer_top_color()->set_b(0xff);

  style->mutable_footer_bottom_color()->set_r(0xee);
  style->mutable_footer_bottom_color()->set_g(0xee);
  style->mutable_footer_bottom_color()->set_b(0xee);

  style->set_logo_file_name("candidate_window_logo.tiff");

  style->mutable_focused_background_color()->set_r(0xd1);
  style->mutable_focused_background_color()->set_g(0xea);
  style->mutable_focused_background_color()->set_b(0xff);

  style->mutable_focused_border_color()->set_r(0x7f);
  style->mutable_focused_border_color()->set_g(0xac);
  style->mutable_focused_border_color()->set_b(0xdd);

  style->mutable_scrollbar_background_color()->set_r(0xe0);
  style->mutable_scrollbar_background_color()->set_g(0xe0);
  style->mutable_scrollbar_background_color()->set_b(0xe0);

  style->mutable_scrollbar_indicator_color()->set_r(0x75);
  style->mutable_scrollbar_indicator_color()->set_g(0x90);
  style->mutable_scrollbar_indicator_color()->set_b(0xb8);

  RendererStyle::InfolistStyle *infostyle = style->mutable_infolist_style();
  // "用例"
  infostyle->set_caption_string("\xE7\x94\xA8\xE4\xBE\x8B");
  infostyle->set_caption_height(20 * scale_factor_y);
  infostyle->set_caption_padding(1);
  infostyle->mutable_caption_style()->set_font_size(12 * scale_factor_y);
  infostyle->mutable_caption_style()->set_left_padding(2 * scale_factor_x);
  infostyle->mutable_caption_background_color()->set_r(0xec);
  infostyle->mutable_caption_background_color()->set_g(0xf0);
  infostyle->mutable_caption_background_color()->set_b(0xfa);

  infostyle->set_window_border(1);  // non-scalable
  infostyle->set_row_rect_padding(2 * scale_factor_x);
  infostyle->set_window_width(300 * scale_factor_x);
  infostyle->mutable_title_style()->set_font_size(15 * scale_factor_y);
  infostyle->mutable_title_style()->set_left_padding(5 * scale_factor_x);
  infostyle->mutable_description_style()->set_font_size(12 * scale_factor_y);
  infostyle->mutable_description_style()->set_left_padding(
      15 * scale_factor_x);
  infostyle->mutable_border_color()->set_r(0x96);
  infostyle->mutable_border_color()->set_g(0x96);
  infostyle->mutable_border_color()->set_b(0x96);
  infostyle->mutable_focused_background_color()->set_r(0xd1);
  infostyle->mutable_focused_background_color()->set_g(0xea);
  infostyle->mutable_focused_background_color()->set_b(0xff);
  infostyle->mutable_focused_border_color()->set_r(0x7f);
  infostyle->mutable_focused_border_color()->set_g(0xac);
  infostyle->mutable_focused_border_color()->set_b(0xdd);
}
} // namespace

bool RendererStyleHandler::GetRendererStyle(RendererStyle *style) {
  return GetRendererStyleHandlerImpl()->GetRendererStyle(style);
}
bool RendererStyleHandler::SetRendererStyle(const RendererStyle &style) {
  return GetRendererStyleHandlerImpl()->SetRendererStyle(style);
}
void RendererStyleHandler::GetDefaultRendererStyle(RendererStyle *style) {
  return GetRendererStyleHandlerImpl()->GetDefaultRendererStyle(style);
}

void RendererStyleHandler::GetDPIScalingFactor(double *x, double *y) {
#if defined OS_WIN
  WTL::CDC desktop_dc(::GetDC(NULL));
  const int dpi_x = desktop_dc.GetDeviceCaps(LOGPIXELSX);
  const int dpi_y = desktop_dc.GetDeviceCaps(LOGPIXELSY);
  if (x != NULL) {
    *x = static_cast<double>(dpi_x) / kDefaultDPI;
  }
  if (y != NULL) {
    *y = static_cast<double>(dpi_y) / kDefaultDPI;
  }
#else
  if (x != NULL) {
    *x = 1.0;
  }
  if (y != NULL) {
    *y = 1.0;
  }
#endif
}

} // namespace renderer
} // namespace mozc
