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

#ifndef MOZC_GUI_CONFIG_DIALOG_KEYMAP_EDITOR_H_
#define MOZC_GUI_CONFIG_DIALOG_KEYMAP_EDITOR_H_

#include <QtGui/QWidget>
#include <set>
#include <string>
#include "base/port.h"
#include "base/scoped_ptr.h"
#include "gui/config_dialog/generic_table_editor.h"

class QAbstractButton;

namespace mozc {
namespace gui {

class ComboBoxDelegate;
class KeyBindingEditorDelegate;

class KeyMapEditorDialog : public GenericTableEditorDialog {
  Q_OBJECT;

 public:
  explicit KeyMapEditorDialog(QWidget *parent);
  virtual ~KeyMapEditorDialog();

  // show a modal dialog
  static bool Show(QWidget *parent,
                   const string &current_keymap,
                   string *new_keymap);

 protected slots:
  virtual void UpdateMenuStatus();
  virtual void OnEditMenuAction(QAction *action);

 protected:
  virtual string GetDefaultFilename() const {
    return "keymap.txt";
  }
  virtual bool LoadFromStream(istream *is);
  virtual bool Update();

 private:
  string invisible_keymap_table_;
  // This is used for deciding whether the user has changed the settings for
  // ime switch or not.
  set<string> ime_switch_keymap_;
  scoped_ptr<QAction *[]> actions_;
  scoped_ptr<QAction *[]> import_actions_;
  scoped_ptr<ComboBoxDelegate> status_delegate_;
  scoped_ptr<ComboBoxDelegate> commands_delegate_;
  scoped_ptr<KeyBindingEditorDelegate> keybinding_delegate_;

  map<string, string> normalized_command_map_;
  map<string, string> normalized_status_map_;
};
}  // namespace gui
}  // namespace mozc
#endif  // MOZC_GUI_CONFIG_DIALOG_KEYMAP_EDITOR_H_
