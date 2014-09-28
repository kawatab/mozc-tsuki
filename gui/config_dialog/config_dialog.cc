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

// Qt component of configure dialog for Mozc
#include "gui/config_dialog/config_dialog.h"

#ifdef OS_WIN
#include <QtGui/QWindowsStyle>
#include <windows.h>
#endif

#include <QtGui/QMessageBox>
#include <algorithm>
#include <cstdlib>
#include <sstream>
#include "base/config_file_stream.h"
#include "base/const.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/run_level.h"
#include "base/system_util.h"
#include "base/util.h"
#include "client/client.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "config/stats_config_util.h"
#include "gui/base/win_util.h"
#include "gui/config_dialog/keymap_editor.h"
#include "gui/config_dialog/roman_table_editor.h"
#include "ipc/ipc.h"
#include "session/commands.pb.h"
#include "session/internal/keymap.h"

namespace {
template<typename T>
void Connect(const QList<T *> &objects,
             const char *signal,
             const QObject *receiver,
             const char *slot) {
  for (typename QList<T *>::const_iterator itr = objects.begin();
       itr != objects.end(); ++itr) {
    QObject::connect(*itr, signal, receiver, slot);
  }
}
}  // namespace

namespace mozc {

namespace gui {
using mozc::config::StatsConfigUtil;

ConfigDialog::ConfigDialog()
    : client_(client::ClientFactory::NewClient()),
      initial_preedit_method_(0),
      initial_use_keyboard_to_change_preedit_method_(false),
      initial_use_mode_indicator_(true) {
  setupUi(this);
  setWindowFlags(Qt::WindowSystemMenuHint);
  setWindowModality(Qt::NonModal);

#ifdef OS_WIN
  miscStartupWidget->setVisible(false);
#endif  // OS_WIN

#ifdef OS_MACOSX
  miscDefaultIMEWidget->setVisible(false);
  miscAdministrationWidget->setVisible(false);
  setWindowTitle(tr("Mozc Preferences"));
#endif  // OS_MACOSX

#if defined(OS_LINUX)
  miscDefaultIMEWidget->setVisible(false);
  miscAdministrationWidget->setVisible(false);
  miscStartupWidget->setVisible(false);
#endif  // OS_LINUX

#ifdef NO_LOGGING
  // disable logging options
  miscLoggingWidget->setVisible(false);

#if defined(OS_LINUX)
  // The last "misc" tab has no valid configs on Linux
  const int kMiscTabIndex = 6;
  configDialogTabWidget->removeTab(kMiscTabIndex);
#endif  // OS_LINUX
#endif  // NO_LOGGING

#ifndef ENABLE_CLOUD_HANDWRITING
  cloudHandwritingCheckBox->setVisible(false);
  cloudServersLayoutWidget->setVisible(false);
  // Hide "Cloud" tab when all the cloud features are not available.
  const int kCloudTabIndex = 5;
  configDialogTabWidget->removeTab(kCloudTabIndex);
#endif  // !ENABLE_CLOUD_HANDWRITING

  suggestionsSizeSpinBox->setRange(1, 9);

  // punctuationsSettingComboBox->addItem(QString::fromUtf8("、。"));
  // punctuationsSettingComboBox->addItem(QString::fromUtf8("，．"));
  // punctuationsSettingComboBox->addItem(QString::fromUtf8("、．"));
  // punctuationsSettingComboBox->addItem(QString::fromUtf8("，。"));
  punctuationsSettingComboBox->addItem
      (QString::fromUtf8("\xE3\x80\x81\xE3\x80\x82"));
  punctuationsSettingComboBox->addItem
      (QString::fromUtf8("\xEF\xBC\x8C\xEF\xBC\x8E"));
  punctuationsSettingComboBox->addItem
      (QString::fromUtf8("\xE3\x80\x81\xEF\xBC\x8E"));
  punctuationsSettingComboBox->addItem
      (QString::fromUtf8("\xEF\xBC\x8C\xE3\x80\x82"));

  // symbolsSettingComboBox->addItem(QString::fromUtf8("「」・"));
  // symbolsSettingComboBox->addItem(QString::fromUtf8("[]／"));
  // symbolsSettingComboBox->addItem(QString::fromUtf8("「」／"));
  // symbolsSettingComboBox->addItem(QString::fromUtf8("[]・"));
  symbolsSettingComboBox->addItem(
      QString::fromUtf8("\xE3\x80\x8C\xE3\x80\x8D\xE3\x83\xBB"));
  symbolsSettingComboBox->addItem(
      QString::fromUtf8("[]\xEF\xBC\x8F"));
  symbolsSettingComboBox->addItem(
      QString::fromUtf8("\xE3\x80\x8C\xE3\x80\x8D\xEF\xBC\x8F"));
  symbolsSettingComboBox->addItem(
      QString::fromUtf8("[]\xE3\x83\xBB"));

  keymapSettingComboBox->addItem(tr("Custom keymap"));
  keymapSettingComboBox->addItem(tr("ATOK"));
  keymapSettingComboBox->addItem(tr("MS-IME"));
  keymapSettingComboBox->addItem(tr("Kotoeri"));

  keymapname_sessionkeymap_map_[tr("ATOK")] = config::Config::ATOK;
  keymapname_sessionkeymap_map_[tr("MS-IME")] = config::Config::MSIME;
  keymapname_sessionkeymap_map_[tr("Kotoeri")] = config::Config::KOTOERI;

  inputModeComboBox->addItem(tr("Romaji"));
  inputModeComboBox->addItem(tr("Kana"));
#ifdef OS_WIN
  // These options changing the preedit method by a hot key are only
  // supported by Windows.
  inputModeComboBox->addItem(tr("Romaji (switchable)"));
  inputModeComboBox->addItem(tr("Kana (switchable)"));
#endif  // OS_WIN

  spaceCharacterFormComboBox->addItem(tr("Follow input mode"));
  spaceCharacterFormComboBox->addItem(tr("Fullwidth"));
  spaceCharacterFormComboBox->addItem(tr("Halfwidth"));

  selectionShortcutModeComboBox->addItem(tr("No shortcut"));
  selectionShortcutModeComboBox->addItem(tr("1 -- 9"));
  selectionShortcutModeComboBox->addItem(tr("A -- L"));

  historyLearningLevelComboBox->addItem(tr("Yes"));
  historyLearningLevelComboBox->addItem(tr("Yes (don't record new data)"));
  historyLearningLevelComboBox->addItem(tr("No"));

  shiftKeyModeSwitchComboBox->addItem(tr("Off"));
  shiftKeyModeSwitchComboBox->addItem(tr("Alphanumeric"));
  shiftKeyModeSwitchComboBox->addItem(tr("Katakana"));

  numpadCharacterFormComboBox->addItem(tr("Follow input mode"));
  numpadCharacterFormComboBox->addItem(tr("Fullwidth"));
  numpadCharacterFormComboBox->addItem(tr("Halfwidth"));
  numpadCharacterFormComboBox->addItem(tr("Direct input"));

  verboseLevelComboBox->addItem(tr("0"));
  verboseLevelComboBox->addItem(tr("1"));
  verboseLevelComboBox->addItem(tr("2"));

  yenSignComboBox->addItem(tr("Yen Sign \xC2\xA5"));
  yenSignComboBox->addItem(tr("Backslash \\"));

#ifndef OS_MACOSX
  // On Windows/Linux, yenSignCombBox can be hidden.
  yenSignLabel->hide();
  yenSignComboBox->hide();
  // On Windows/Linux, useJapaneseLayout checkbox should be invisible.
  useJapaneseLayout->hide();
#endif  // !OS_MACOSX

#ifndef MOZC_ENABLE_MODE_INDICATOR
  // If not enabled, useModeIndicator checkbox should be invisible.
  useModeIndicator->hide();
#endif  // !MOZC_ENABLE_MODE_INDICATOR

  // signal/slot
  QObject::connect(configDialogButtonBox,
                   SIGNAL(clicked(QAbstractButton *)),
                   this,
                   SLOT(clicked(QAbstractButton *)));
  QObject::connect(clearUserHistoryButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(ClearUserHistory()));
  QObject::connect(clearUserPredictionButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(ClearUserPrediction()));
  QObject::connect(clearUnusedUserPredictionButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(ClearUnusedUserPrediction()));
  QObject::connect(editUserDictionaryButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(EditUserDictionary()));
  QObject::connect(editKeymapButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(EditKeymap()));
  QObject::connect(resetToDefaultsButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(ResetToDefaults()));
  QObject::connect(editRomanTableButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(EditRomanTable()));
  QObject::connect(inputModeComboBox,
                   SIGNAL(currentIndexChanged(int)),
                   this,
                   SLOT(SelectInputModeSetting(int)));
  QObject::connect(useAutoConversion,
                   SIGNAL(stateChanged(int)),
                   this,
                   SLOT(SelectAutoConversionSetting(int)));
  QObject::connect(historySuggestCheckBox,
                   SIGNAL(stateChanged(int)),
                   this,
                   SLOT(SelectSuggestionSetting(int)));
  QObject::connect(dictionarySuggestCheckBox,
                   SIGNAL(stateChanged(int)),
                   this,
                   SLOT(SelectSuggestionSetting(int)));
  QObject::connect(realtimeConversionCheckBox,
                   SIGNAL(stateChanged(int)),
                   this,
                   SLOT(SelectSuggestionSetting(int)));
  QObject::connect(launchAdministrationDialogButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(LaunchAdministrationDialog()));
  QObject::connect(launchAdministrationDialogButtonForUsageStats,
                   SIGNAL(clicked()),
                   this,
                   SLOT(LaunchAdministrationDialog()));

  // Event handlers to enable 'Apply' button.
  Connect(findChildren<QPushButton *>(), SIGNAL(clicked()), this,
          SLOT(EnableApplyButton()));
  Connect(findChildren<QCheckBox *>(), SIGNAL(clicked()), this,
          SLOT(EnableApplyButton()));
  Connect(findChildren<QComboBox *>(), SIGNAL(activated(int)), this,
          SLOT(EnableApplyButton()));
  Connect(findChildren<QSpinBox *>(), SIGNAL(editingFinished()), this,
          SLOT(EnableApplyButton()));
  // 'Apply' button is disabled on launching.
  configDialogButtonBox->button(QDialogButtonBox::Apply)->setEnabled(false);

  // When clicking these messages, CheckBoxs corresponding
  // to them should be toggled.
  // We cannot use connect/slot as QLabel doesn't define
  // clicked slot by default.
  usageStatsMessage->installEventFilter(this);
  incognitoModeMessage->installEventFilter(this);

#ifndef OS_WIN
  checkDefaultCheckBox->setVisible(false);
  checkDefaultLine->setVisible(false);
  checkDefaultLabel->setVisible(false);
#endif   // !OS_WIN

#ifdef OS_WIN
  launchAdministrationDialogButton->setEnabled(true);
  // if the current application is not elevated by UAC,
  // add a shield icon
  if (mozc::SystemUtil::IsVistaOrLater()) {
    if (!mozc::RunLevel::IsElevatedByUAC()) {
      QWindowsStyle style;
      QIcon vista_icon(style.standardIcon(QStyle::SP_VistaShield));
      launchAdministrationDialogButton->setIcon(vista_icon);
      launchAdministrationDialogButtonForUsageStats->setIcon(vista_icon);
    }
  } else {
    dictionaryPreloadingAndUACLabel->setText(
        tr("Dictionary preloading"));
  }

  usageStatsCheckBox->setDisabled(true);
  usageStatsCheckBox->setVisible(false);
  usageStatsMessage->setDisabled(true);
  usageStatsMessage->setVisible(false);
#else
  launchAdministrationDialogButton->setEnabled(false);
  launchAdministrationDialogButton->setVisible(false);
  launchAdministrationDialogButtonForUsageStats->setEnabled(false);
  launchAdministrationDialogButtonForUsageStats->setVisible(false);
  administrationLine->setVisible(false);
  administrationLabel->setVisible(false);
  dictionaryPreloadingAndUACLabel->setVisible(false);
#endif  // OS_WIN

#ifdef OS_LINUX
  // On Linux, disable all fields for UsageStats
  usageStatsLabel->setEnabled(false);
  usageStatsLabel->setVisible(false);
  usageStatsLine->setEnabled(false);
  usageStatsLine->setVisible(false);
  usageStatsMessage->setEnabled(false);
  usageStatsMessage->setVisible(false);
  usageStatsCheckBox->setEnabled(false);
  usageStatsCheckBox->setVisible(false);
#endif  // OS_LINUX

  Reload();

#ifdef OS_WIN
  IMEHotKeyDisabledCheckBox->setChecked(WinUtil::GetIMEHotKeyDisabled());
#else
  IMEHotKeyDisabledCheckBox->setVisible(false);
#endif

#ifdef CHANNEL_DEV
  usageStatsCheckBox->setEnabled(false);
#endif  // CHANNEL_DEV
}

ConfigDialog::~ConfigDialog() {
}

bool ConfigDialog::SetConfig(const config::Config &config) {
  if (!client_->CheckVersionOrRestartServer()) {
    LOG(ERROR) << "CheckVersionOrRestartServer failed";
    return false;
  }

  if (!client_->SetConfig(config)) {
    LOG(ERROR) << "SetConfig failed";
    return false;
  }

  return true;
}

bool ConfigDialog::GetConfig(config::Config *config) {
  if (!client_->CheckVersionOrRestartServer()) {
    LOG(ERROR) << "CheckVersionOrRestartServer failed";
    return false;
  }

  if (!client_->GetConfig(config)) {
    LOG(ERROR) << "GetConfig failed";
    return false;
  }

  return true;
}

void ConfigDialog::Reload() {
  config::Config config;
  if (!GetConfig(&config)) {
    QMessageBox::critical(this, windowTitle(),
                          tr("Failed to get current config values."));
  }
  ConvertFromProto(config);

  SelectAutoConversionSetting(static_cast<int>(config.use_auto_conversion()));

  initial_preedit_method_ = static_cast<int>(config.preedit_method());
  initial_use_keyboard_to_change_preedit_method_ =
      config.use_keyboard_to_change_preedit_method();
  initial_use_mode_indicator_ = config.use_mode_indicator();
}

bool ConfigDialog::Update() {
  config::Config config;
  ConvertToProto(&config);

  if (config.session_keymap() == config::Config::CUSTOM &&
      config.custom_keymap_table().empty()) {
    QMessageBox::warning(this, windowTitle(),
        tr("The current custom keymap table is empty. "
           "When custom keymap is selected, "
           "you must customize it."));
    return false;
  }


#if defined(OS_WIN)
  if ((initial_preedit_method_ !=
       static_cast<int>(config.preedit_method())) ||
      (initial_use_keyboard_to_change_preedit_method_ !=
       config.use_keyboard_to_change_preedit_method())) {
    QMessageBox::information(this, windowTitle(),
                             tr("Romaji/Kana setting is enabled from"
                                " new applications."));
    initial_preedit_method_ = static_cast<int>(config.preedit_method());
    initial_use_keyboard_to_change_preedit_method_ =
        config.use_keyboard_to_change_preedit_method();
  }
#endif  // OS_WIN

#ifdef OS_WIN
  if (initial_use_mode_indicator_ != config.use_mode_indicator()) {
    QMessageBox::information(this, windowTitle(),
                             tr("Input mode indicator setting is enabled from"
                                " new applications."));
    initial_use_mode_indicator_ = config.use_mode_indicator();
  }
#endif  // OS_WIN

  if (!SetConfig(config)) {
    QMessageBox::critical(this, windowTitle(),
                          tr("Failed to update config"));
  }

#ifdef OS_WIN
  if (!WinUtil::SetIMEHotKeyDisabled(IMEHotKeyDisabledCheckBox->isChecked())) {
    // Do not show any dialog here, since this operation will not fail
    // in almost all cases.
    // TODO(taku): better to show dialog?
    LOG(ERROR) << "Failed to update IME HotKey status";
  }
#endif  // OS_WIN

#ifdef OS_MACOSX
  if (startupCheckBox->isChecked()) {
    if (!MacUtil::CheckPrelauncherLoginItemStatus()) {
      MacUtil::AddPrelauncherLoginItem();
    }
  } else {
    if (MacUtil::CheckPrelauncherLoginItemStatus()) {
      MacUtil::RemovePrelauncherLoginItem();
    }
  }
#endif  // OS_MACOSX

  return true;
}

void ConfigDialog::SetSendStatsCheckBox() {
  // On windows, usage_stats flag is managed by
  // administration_dialog. http://b/2889759
#ifndef OS_WIN
  const bool val = StatsConfigUtil::IsEnabled();
  usageStatsCheckBox->setChecked(val);
#endif  // OS_WIN
}

void ConfigDialog::GetSendStatsCheckBox() const {
  // On windows, usage_stats flag is managed by
  // administration_dialog. http://b/2889759
#ifndef OS_WIN
  const bool val = usageStatsCheckBox->isChecked();
  StatsConfigUtil::SetEnabled(val);
#endif  // OS_WIN
}

#define SET_COMBOBOX(combobox, enumname, field) \
do { \
  (combobox)->setCurrentIndex(static_cast<int>(config.field()));  \
} while (0)

#define SET_CHECKBOX(checkbox, field) \
do { (checkbox)->setChecked(config.field()); } while (0)

#define GET_COMBOBOX(combobox, enumname, field) \
do {  \
  config->set_##field(static_cast<config::Config_##enumname> \
                   ((combobox)->currentIndex())); \
} while (0)

#define GET_CHECKBOX(checkbox, field) \
do { config->set_##field((checkbox)->isChecked());  } while (0)


namespace {
static const int kPreeditMethodSize = 2;

void SetComboboxForPreeditMethod(const config::Config &config,
                                 QComboBox *combobox) {
  int index = static_cast<int>(config.preedit_method());
#ifdef OS_WIN
  if (config.use_keyboard_to_change_preedit_method()) {
    index += kPreeditMethodSize;
  }
#endif  // OS_WIN
  combobox->setCurrentIndex(index);
}

void GetComboboxForPreeditMethod(const QComboBox *combobox,
                                 config::Config *config) {
  int index = combobox->currentIndex();
  if (index >= kPreeditMethodSize) {
    // |use_keyboard_to_change_preedit_method| should be true and
    // |index| should be adjusted to smaller than kPreeditMethodSize.
    config->set_preedit_method(
        static_cast<config::Config_PreeditMethod>(index - kPreeditMethodSize));
    config->set_use_keyboard_to_change_preedit_method(true);
  } else {
    config->set_preedit_method(
        static_cast<config::Config_PreeditMethod>(index));
    config->set_use_keyboard_to_change_preedit_method(false);
  }
}
}  // anonymous namespace


// TODO(taku)
// Actually ConvertFromProto and ConvertToProto are almost the same.
// The difference only SET_ and GET_. We would like to unify the twos.
void ConfigDialog::ConvertFromProto(const config::Config &config) {
  // tab1
  SetComboboxForPreeditMethod(config, inputModeComboBox);
  SET_COMBOBOX(punctuationsSettingComboBox, PunctuationMethod,
               punctuation_method);
  SET_COMBOBOX(symbolsSettingComboBox, SymbolMethod, symbol_method);
  SET_COMBOBOX(spaceCharacterFormComboBox, FundamentalCharacterForm,
               space_character_form);
  SET_COMBOBOX(selectionShortcutModeComboBox, SelectionShortcut,
               selection_shortcut);
  SET_COMBOBOX(numpadCharacterFormComboBox, NumpadCharacterForm,
               numpad_character_form);
  SET_COMBOBOX(keymapSettingComboBox, SessionKeymap, session_keymap);

  custom_keymap_table_ = config.custom_keymap_table();
  custom_roman_table_ = config.custom_roman_table();

  // tab2
  SET_COMBOBOX(historyLearningLevelComboBox, HistoryLearningLevel,
               history_learning_level);
  SET_CHECKBOX(singleKanjiConversionCheckBox, use_single_kanji_conversion);
  SET_CHECKBOX(symbolConversionCheckBox, use_symbol_conversion);
  SET_CHECKBOX(emoticonConversionCheckBox, use_emoticon_conversion);
  SET_CHECKBOX(dateConversionCheckBox, use_date_conversion);
  SET_CHECKBOX(emojiConversionCheckBox, use_emoji_conversion);
  SET_CHECKBOX(numberConversionCheckBox, use_number_conversion);
  SET_CHECKBOX(calculatorCheckBox, use_calculator);
  SET_CHECKBOX(t13nConversionCheckBox, use_t13n_conversion);
  SET_CHECKBOX(zipcodeConversionCheckBox, use_zip_code_conversion);
  SET_CHECKBOX(spellingCorrectionCheckBox, use_spelling_correction);

  // InfoListConfig
  localUsageDictionaryCheckBox->setChecked(
      config.information_list_config().use_local_usage_dictionary());
  information_list_config_.CopyFrom(config.information_list_config());

  // tab3
  SET_CHECKBOX(useAutoImeTurnOff, use_auto_ime_turn_off);

  SET_CHECKBOX(useAutoConversion, use_auto_conversion);
  kutenCheckBox->setChecked(
      config.auto_conversion_key() &
      config::Config::AUTO_CONVERSION_KUTEN);
  toutenCheckBox->setChecked(
      config.auto_conversion_key() &
      config::Config::AUTO_CONVERSION_TOUTEN);
  questionMarkCheckBox->setChecked(
      config.auto_conversion_key() &
      config::Config::AUTO_CONVERSION_QUESTION_MARK);
  exclamationMarkCheckBox->setChecked(
      config.auto_conversion_key() &
      config::Config::AUTO_CONVERSION_EXCLAMATION_MARK);

  SET_COMBOBOX(shiftKeyModeSwitchComboBox,
               ShiftKeyModeSwitch,
               shift_key_mode_switch);

  SET_CHECKBOX(useJapaneseLayout, use_japanese_layout);

  SET_CHECKBOX(useModeIndicator, use_mode_indicator);

  // tab4
  SET_CHECKBOX(historySuggestCheckBox, use_history_suggest);
  SET_CHECKBOX(dictionarySuggestCheckBox, use_dictionary_suggest);
  SET_CHECKBOX(realtimeConversionCheckBox, use_realtime_conversion);

  suggestionsSizeSpinBox->setValue
      (max(1, min(9, static_cast<int>(config.suggestions_size()))));

  // tab5
  SetSendStatsCheckBox();
  SET_CHECKBOX(incognitoModeCheckBox, incognito_mode);
  SET_CHECKBOX(presentationModeCheckBox, presentation_mode);

  // tab6
  SET_COMBOBOX(verboseLevelComboBox, int, verbose_level);
  SET_CHECKBOX(checkDefaultCheckBox, check_default);
  SET_COMBOBOX(yenSignComboBox, YenSignCharacter, yen_sign_character);

  characterFormEditor->Load(config);
  SET_CHECKBOX(cloudHandwritingCheckBox, allow_cloud_handwriting);

#ifdef OS_MACOSX
  startupCheckBox->setChecked(
      MacUtil::CheckPrelauncherLoginItemStatus());
#endif  // OS_MACOSX
}

void ConfigDialog::ConvertToProto(config::Config *config) const {
  // tab1
  GetComboboxForPreeditMethod(inputModeComboBox, config);
  GET_COMBOBOX(punctuationsSettingComboBox, PunctuationMethod,
               punctuation_method);
  GET_COMBOBOX(symbolsSettingComboBox, SymbolMethod, symbol_method);
  GET_COMBOBOX(spaceCharacterFormComboBox, FundamentalCharacterForm,
               space_character_form);
  GET_COMBOBOX(selectionShortcutModeComboBox, SelectionShortcut,
               selection_shortcut);
  GET_COMBOBOX(numpadCharacterFormComboBox, NumpadCharacterForm,
               numpad_character_form);
  GET_COMBOBOX(keymapSettingComboBox, SessionKeymap, session_keymap);

  config->set_custom_keymap_table(custom_keymap_table_);

  config->clear_custom_roman_table();
  if (!custom_roman_table_.empty()) {
    config->set_custom_roman_table(custom_roman_table_);
  }

  // tab2
  GET_COMBOBOX(historyLearningLevelComboBox, HistoryLearningLevel,
               history_learning_level);
  GET_CHECKBOX(singleKanjiConversionCheckBox, use_single_kanji_conversion);
  GET_CHECKBOX(symbolConversionCheckBox, use_symbol_conversion);
  GET_CHECKBOX(emoticonConversionCheckBox, use_emoticon_conversion);
  GET_CHECKBOX(dateConversionCheckBox, use_date_conversion);
  GET_CHECKBOX(emojiConversionCheckBox, use_emoji_conversion);
  GET_CHECKBOX(numberConversionCheckBox, use_number_conversion);
  GET_CHECKBOX(calculatorCheckBox, use_calculator);
  GET_CHECKBOX(t13nConversionCheckBox, use_t13n_conversion);
  GET_CHECKBOX(zipcodeConversionCheckBox, use_zip_code_conversion);
  GET_CHECKBOX(spellingCorrectionCheckBox, use_spelling_correction);

  // InformationListConfig
  config->mutable_information_list_config()->CopyFrom(
      information_list_config_);
  config->mutable_information_list_config()->set_use_local_usage_dictionary(
      localUsageDictionaryCheckBox->isChecked());

  // tab3
  GET_CHECKBOX(useAutoImeTurnOff, use_auto_ime_turn_off);

  GET_CHECKBOX(useAutoConversion, use_auto_conversion);

  GET_CHECKBOX(useJapaneseLayout, use_japanese_layout);

  GET_CHECKBOX(useModeIndicator, use_mode_indicator);

  uint32 auto_conversion_key = 0;
  if (kutenCheckBox->isChecked()) {
    auto_conversion_key |=
        config::Config::AUTO_CONVERSION_KUTEN;
  }
  if (toutenCheckBox->isChecked()) {
    auto_conversion_key |=
        config::Config::AUTO_CONVERSION_TOUTEN;
  }
  if (questionMarkCheckBox->isChecked()) {
    auto_conversion_key |=
        config::Config::AUTO_CONVERSION_QUESTION_MARK;
  }
  if (exclamationMarkCheckBox->isChecked()) {
    auto_conversion_key |=
        config::Config::AUTO_CONVERSION_EXCLAMATION_MARK;
  }
  config->set_auto_conversion_key(auto_conversion_key);

  GET_COMBOBOX(shiftKeyModeSwitchComboBox,
               ShiftKeyModeSwitch,
               shift_key_mode_switch);

  // tab4
  GET_CHECKBOX(historySuggestCheckBox, use_history_suggest);
  GET_CHECKBOX(dictionarySuggestCheckBox, use_dictionary_suggest);
  GET_CHECKBOX(realtimeConversionCheckBox, use_realtime_conversion);

  config->set_suggestions_size
      (static_cast<uint32>(suggestionsSizeSpinBox->value()));

  // tab5
  GetSendStatsCheckBox();
  GET_CHECKBOX(incognitoModeCheckBox, incognito_mode);
  GET_CHECKBOX(presentationModeCheckBox, presentation_mode);

  // tab6
  config->set_verbose_level(verboseLevelComboBox->currentIndex());
  GET_CHECKBOX(checkDefaultCheckBox, check_default);
  GET_COMBOBOX(yenSignComboBox, YenSignCharacter, yen_sign_character);

  characterFormEditor->Save(config);
#ifdef ENABLE_CLOUD_HANDWRITING
  GET_CHECKBOX(cloudHandwritingCheckBox, allow_cloud_handwriting);
#endif  // ENABLE_CLOUD_HANDWRITING
}

#undef SET_COMBOBOX
#undef SET_CHECKBOX
#undef GET_COMBOBOX
#undef GET_CHECKBOX

void ConfigDialog::clicked(QAbstractButton *button) {
  switch (configDialogButtonBox->buttonRole(button)) {
    case QDialogButtonBox::AcceptRole:
      if (Update()) {
        QWidget::close();
      }
      break;
    case QDialogButtonBox::ApplyRole:
      Update();
      break;
    case QDialogButtonBox::RejectRole:
      QWidget::close();
      break;
    default:
      break;
  }
}

void ConfigDialog::ClearUserHistory() {
  if (QMessageBox::Ok !=
      QMessageBox::question(
          this,
          windowTitle(),
          tr("Do you want to clear personalization data? "
             "Input history is not reset with this operation. "
             "Please open \"suggestion\" tab to remove input history data."),
          QMessageBox::Ok | QMessageBox::Cancel,
          QMessageBox::Cancel)) {
    return;
  }

  client_->CheckVersionOrRestartServer();

  if (!client_->ClearUserHistory()) {
    QMessageBox::critical(
        this,
        windowTitle(),
        tr("Mozc Converter is not running. "
           "Settings were not saved."));
  }
}

void ConfigDialog::ClearUserPrediction() {
  if (QMessageBox::Ok !=
      QMessageBox::question(
          this,
          windowTitle(),
          tr("Do you want to clear all history data?"),
          QMessageBox::Ok | QMessageBox::Cancel,
          QMessageBox::Cancel)) {
    return;
  }

  client_->CheckVersionOrRestartServer();

  if (!client_->ClearUserPrediction()) {
    QMessageBox::critical(
        this,
        windowTitle(),
        tr("Mozc Converter is not running. "
           "Settings were not saved."));
  }
}

void ConfigDialog::ClearUnusedUserPrediction() {
  if (QMessageBox::Ok !=
      QMessageBox::question(
          this,
          windowTitle(),
          tr("Do you want to clear unused history data?"),
          QMessageBox::Ok | QMessageBox::Cancel,
          QMessageBox::Cancel)) {
    return;
  }

  client_->CheckVersionOrRestartServer();

  if (!client_->ClearUnusedUserPrediction()) {
    QMessageBox::critical(
        this,
        windowTitle(),
        tr("Mozc Converter is not running. "
           "Operation was not executed."));
  }
}

void ConfigDialog::EditUserDictionary() {
  client_->LaunchTool("dictionary_tool", "");
}

void ConfigDialog::EditKeymap() {
  string current_keymap_table = "";
  const QString keymap_name = keymapSettingComboBox->currentText();
  const map<QString, config::Config::SessionKeymap>::const_iterator itr =
      keymapname_sessionkeymap_map_.find(keymap_name);
  if (itr != keymapname_sessionkeymap_map_.end()) {
    // Load from predefined mapping file.
    const char *keymap_file =
        keymap::KeyMapManager::GetKeyMapFileName(itr->second);
    scoped_ptr<istream> ifs(
        ConfigFileStream::LegacyOpen(keymap_file));
    CHECK(ifs.get() != NULL);  // should never happen
    stringstream buffer;
    buffer << ifs->rdbuf();
    current_keymap_table = buffer.str();
  } else {
    current_keymap_table = custom_keymap_table_;
  }
  string output;
  if (gui::KeyMapEditorDialog::Show(this,
                                    current_keymap_table,
                                    &output)) {
    custom_keymap_table_ = output;
    // set keymapSettingComboBox to "Custom keymap"
    keymapSettingComboBox->setCurrentIndex(0);
  }
}

void ConfigDialog::EditRomanTable() {
  string output;
  if (gui::RomanTableEditorDialog::Show(this,
                                        custom_roman_table_,
                                        &output)) {
    custom_roman_table_ = output;
  }
}

void ConfigDialog::SelectInputModeSetting(int index) {
  // enable "EDIT" button if roman mode is selected
  editRomanTableButton->setEnabled((index == 0));
}

void ConfigDialog::SelectAutoConversionSetting(int state) {
  kutenCheckBox->setEnabled(static_cast<bool>(state));
  toutenCheckBox->setEnabled(static_cast<bool>(state));
  questionMarkCheckBox->setEnabled(static_cast<bool>(state));
  exclamationMarkCheckBox->setEnabled(static_cast<bool>(state));
}

void ConfigDialog::SelectSuggestionSetting(int state) {
  if (historySuggestCheckBox->isChecked() ||
      dictionarySuggestCheckBox->isChecked() ||
      realtimeConversionCheckBox->isChecked()) {
    presentationModeCheckBox->setEnabled(true);
  } else {
    presentationModeCheckBox->setEnabled(false);
  }
}

void ConfigDialog::ResetToDefaults() {
  if (QMessageBox::Ok ==
      QMessageBox::question(
          this,
          windowTitle(),
          tr("When you reset Mozc settings, any changes "
             "you've made will be reverted to the default settings. "
             "Do you want to reset settings? "
             "The following items are not reset with this operation.\n"
             " - Personalization data\n"
             " - Input history\n"
             " - Usage statistics and crash reports\n"
             " - Administrator settings"),
          QMessageBox::Ok | QMessageBox::Cancel,
          QMessageBox::Cancel)) {
    // TODO(taku): remove the dependency to config::ConfigHandler
    // nice to have GET_DEFAULT_CONFIG command
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    ConvertFromProto(config);
  }
}

void ConfigDialog::LaunchAdministrationDialog() {
#ifdef OS_WIN
  client_->LaunchTool("administration_dialog", "");
#endif  // OS_WIN
}

void ConfigDialog::EnableApplyButton() {
  configDialogButtonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
}

// Catch MouseButtonRelease event to toggle the CheckBoxes
bool ConfigDialog::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::MouseButtonRelease) {
    if (obj == usageStatsMessage) {
#ifndef CHANNEL_DEV
      usageStatsCheckBox->toggle();
#endif
    } else if (obj == incognitoModeMessage) {
      incognitoModeCheckBox->toggle();
    }
  }
  return QObject::eventFilter(obj, event);
}

}  // namespace gui
}  // namespace mozc
