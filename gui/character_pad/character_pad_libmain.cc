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

#ifdef OS_WIN
#include <windows.h>
#endif

#include <QtGui/QApplication>
#include <QtCore/QFile>
#include "base/logging.h"
#include "base/system_util.h"
#include "base/util.h"
#include "gui/base/locale_util.h"
#include "gui/base/win_util.h"
#include "gui/character_pad/character_palette.h"
#include "gui/character_pad/hand_writing.h"
#include "gui/character_pad/selection_handler.h"
#include "gui/character_pad/windows_selection_handler.h"
#include "handwriting/handwriting_manager.h"
#include "handwriting/zinnia_handwriting.h"

namespace {

#ifdef OS_WIN
void InstallStyleSheet(const string &style_sheet) {
  QFile file(style_sheet.c_str());
  file.open(QFile::ReadOnly);
  qApp->setStyleSheet(QLatin1String(file.readAll()));
}
#endif  // OS_WIN

enum {
  CHARACTER_PALETTE,
  HAND_WRITING
};
}  // namespace

int RunCharacterPad(int argc, char *argv[],
                    int mode) {
  Q_INIT_RESOURCE(qrc_character_pad);
  QApplication app(argc, argv);

  mozc::SystemUtil::DisableIME();

  mozc::gui::LocaleUtil::InstallTranslationMessageAndFont("character_pad");

  scoped_ptr<QMainWindow> window;

  if (mode == HAND_WRITING) {
    window.reset(new mozc::gui::HandWriting);
  } else if (mode == CHARACTER_PALETTE) {
    window.reset(new mozc::gui::CharacterPalette);
  }
  CHECK(window.get());

#ifdef OS_WIN
  mozc::gui::WindowsSelectionHandler callback;
  mozc::gui::SelectionHandler::SetSelectionCallback(&callback);

  window->setWindowFlags(Qt::WindowSystemMenuHint);

  // Set Top-Most bit:
  //   Use SWP_NOACTIVATE so that the GUI window will not get focus from the
  //   application which is currently active. b/5516521
  ::SetWindowPos(window->winId(), HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

  // Set WS_EX_NOACTIVATE so that the GUI window will not be activated by mouse
  // click.
  const LONG style = ::GetWindowLong(window->winId(), GWL_EXSTYLE)
                     | WS_EX_NOACTIVATE | WS_EX_APPWINDOW;
  ::SetWindowLong(window->winId(), GWL_EXSTYLE, style);

  // Aero
  if (mozc::SystemUtil::IsVistaOrLater()) {
    window->setContentsMargins(0, 0, 0, 0);
    mozc::gui::WinUtil::InstallStyleSheetsFiles(
        ":character_pad_win_aero_style.qss",
        ":character_pad_win_style.qss");
    if (mozc::gui::WinUtil::IsCompositionEnabled()) {
      mozc::gui::WinUtil::ExtendFrameIntoClientArea(window.get());
      InstallStyleSheet(":character_pad_win_aero_style.qss");
    } else {
      InstallStyleSheet(":character_pad_win_style.qss");
    }
  }
#endif

  window->show();
  window->raise();

  return app.exec();
}

int RunCharacterPalette(int argc, char *argv[]) {
  return RunCharacterPad(argc, argv, CHARACTER_PALETTE);
}

int RunHandWriting(int argc, char *argv[]) {
  mozc::handwriting::ZinniaHandwriting zinnia_handwriting(
      mozc::handwriting::ZinniaHandwriting::GetModelFileName());
  mozc::handwriting::HandwritingManager::SetHandwritingModule(
      &zinnia_handwriting);
  return RunCharacterPad(argc, argv, HAND_WRITING);
}
