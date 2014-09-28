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

#include "gui/dictionary_tool/import_dialog.h"

#include <QtGui/QtGui>
#include "base/util.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_importer.h"

namespace mozc {
namespace gui {

ImportDialog::ImportDialog(QWidget *parent)
    : QDialog(parent,
              // To disable context help on Windows.
              Qt::WindowTitleHint | Qt::WindowSystemMenuHint) {
  setupUi(this);

#ifdef OS_MACOSX
  layout()->setContentsMargins(8, 12, 8, 8);
#endif

  // Initialize combo boxes for lists of IMEs and character encodings.
  ime_combobox_->addItem(
      tr("Auto detection"),
      static_cast<int>(UserDictionaryImporter::IME_AUTO_DETECT));
  ime_combobox_->addItem(
      tr("Google"),
      static_cast<int>(UserDictionaryImporter::MOZC));

#ifdef OS_WIN
  ime_combobox_->addItem(
      tr("Microsoft IME"),
      static_cast<int>(UserDictionaryImporter::MSIME));
  ime_combobox_->addItem(
      tr("ATOK"),
      static_cast<int>(UserDictionaryImporter::ATOK));
  ime_combobox_->addItem(
      tr("Kotoeri"),
      static_cast<int>(UserDictionaryImporter::KOTOERI));
#else
  ime_combobox_->addItem(
      tr("Kotoeri"),
      static_cast<int>(UserDictionaryImporter::KOTOERI));
  ime_combobox_->addItem(
      tr("ATOK"),
      static_cast<int>(UserDictionaryImporter::ATOK));
  ime_combobox_->addItem(
      tr("Microsoft IME"),
      static_cast<int>(UserDictionaryImporter::MSIME));
#endif

  encoding_combobox_->addItem(
      tr("Auto detection"),
      static_cast<int>(UserDictionaryImporter::ENCODING_AUTO_DETECT));
  encoding_combobox_->addItem(
      tr("Unicode"),
      static_cast<int>(UserDictionaryImporter::UTF16));
  encoding_combobox_->addItem(
      tr("Shift JIS"),
      static_cast<int>(UserDictionaryImporter::SHIFT_JIS));
  encoding_combobox_->addItem(
      tr("UTF-8"),
      static_cast<int>(UserDictionaryImporter::UTF8));

  QPushButton *button = buttonbox_->button(QDialogButtonBox::Ok);
  if (button != NULL) {
    button->setText(tr("Import"));
  }

  // Signals and slots to connect buttons and actions.
  connect(select_file_pushbutton_, SIGNAL(clicked()),
          this, SLOT(SelectFile()));

  connect(buttonbox_,
          SIGNAL(clicked(QAbstractButton *)),
          this,
          SLOT(Clicked(QAbstractButton *)));

  // Signals and slots to manage availability of GUI components.
  connect(file_name_lineedit_, SIGNAL(textChanged(const QString &)),
          this, SLOT(OnFormValueChanged()));
  connect(dic_name_lineedit_, SIGNAL(textChanged(const QString &)),
          this, SLOT(OnFormValueChanged()));
}

ImportDialog::~ImportDialog() {}

const QString ImportDialog::file_name() const {
  return file_name_lineedit_->text();
}

const QString ImportDialog::dic_name() const {
  return dic_name_lineedit_->text();
}

UserDictionaryImporter::IMEType ImportDialog::ime_type() const {
  return static_cast<UserDictionaryImporter::IMEType>(
      ime_combobox_->itemData(ime_combobox_->currentIndex()).toInt());
}

UserDictionaryImporter::EncodingType ImportDialog::encoding_type() const {
  return static_cast<UserDictionaryImporter::EncodingType>(
      encoding_combobox_->itemData(encoding_combobox_->currentIndex()).toInt());
}

int ImportDialog::ExecInCreateMode() {
  mode_ = CREATE;
  Reset();
  return exec();
}

int ImportDialog::ExecInAppendMode() {
  mode_ = APPEND;
  Reset();
  return exec();
}

bool ImportDialog::IsAcceptButtonEnabled() const {
  const bool is_enabled =
      (mode_ == CREATE &&
       !file_name_lineedit_->text().isEmpty() &&
       !dic_name_lineedit_->text().isEmpty()) ||
      (mode_ == APPEND &&
       !file_name_lineedit_->text().isEmpty());
  return is_enabled;
}

void ImportDialog::OnFormValueChanged() {
  QPushButton *button = buttonbox_->button(QDialogButtonBox::Ok);
  if (button != NULL) {
    button->setEnabled(IsAcceptButtonEnabled());
  }
}

void ImportDialog::Reset() {
  file_name_lineedit_->clear();
  dic_name_lineedit_->clear();
  ime_combobox_->setCurrentIndex(0);
  encoding_combobox_->setCurrentIndex(0);

  if (mode_ == CREATE) {
    dic_name_lineedit_->show();
    dic_name_label_->show();
  } else {
    dic_name_lineedit_->hide();
    dic_name_label_->hide();
  }

  OnFormValueChanged();
  file_name_lineedit_->setFocus();
}

void ImportDialog::SelectFile() {
  const QString initial_path =
      file_name_lineedit_->text().isEmpty() ?
      QDir::homePath() :
      file_name_lineedit_->text();
  const QString filename = QFileDialog::getOpenFileName(
      this, tr("Import dictionary"),
      initial_path,
      tr("Text Files (*.txt);;All Files (*)"));
  if (filename.isEmpty()) {
    return;
  }
  file_name_lineedit_->setText(
      QDir::toNativeSeparators(filename));
}

void ImportDialog::Clicked(QAbstractButton *button) {
  switch (buttonbox_->buttonRole(button)) {
    case QDialogButtonBox::AcceptRole:
      if (IsAcceptButtonEnabled()) {
        accept();
      }
      break;
    case QDialogButtonBox::RejectRole:
      reject();
      break;
    default:
      break;
  }
}
}  // namespace gui
}  // namespace mozc
