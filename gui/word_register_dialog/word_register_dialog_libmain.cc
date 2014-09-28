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

#include <QtGui/QApplication>
#include <QtGui/QDialog>
#include <QtGui/QtGui>
#include "base/system_util.h"
#include "gui/base/locale_util.h"
#include "gui/base/singleton_window_helper.h"
#include "gui/word_register_dialog/word_register_dialog.h"

int RunWordRegisterDialog(int argc, char *argv[]) {
  Q_INIT_RESOURCE(qrc_word_register_dialog);
  QApplication app(argc, argv);

  string name = "word_register_dialog.";
  name += mozc::SystemUtil::GetDesktopNameAsString();

  mozc::gui::SingletonWindowHelper window_helper(name);
  if (window_helper.FindPreviousWindow()) {
    // already running
    window_helper.ActivatePreviousWindow();
    return -1;
  }

  mozc::gui::LocaleUtil::InstallTranslationMessageAndFont(
      "word_register_dialog");

  mozc::gui::WordRegisterDialog word_register_dialog;
  if (!word_register_dialog.IsAvailable()) {
    return -1;
  }

  word_register_dialog.show();
  word_register_dialog.raise();

  return app.exec();
}
