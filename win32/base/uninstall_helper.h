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

#ifndef MOZC_WIN32_BASE_UNINSTALL_HELPER_H_
#define MOZC_WIN32_BASE_UNINSTALL_HELPER_H_

#include <windows.h>

#include <vector>

#include "base/port.h"
#include "win32/base/input_dll.h"
#include "win32/base/keyboard_layout_id.h"
#include "testing/base/public/gunit_prod.h"
// for FRIEND_TEST()

namespace mozc {
namespace win32 {
struct KeyboardLayoutInfo {
  KeyboardLayoutInfo();
  DWORD klid;
  wstring ime_filename;
};

struct LayoutProfileInfo {
  LayoutProfileInfo();
  LANGID  langid;
  CLSID   clsid;
  GUID    profile_guid;
  DWORD   klid;
  wstring ime_filename;
  bool    is_default;
  bool    is_tip;
  bool    is_enabled;
};

// This class is used to determine the new enabled layout/profile for the
// current user after Google Japanese Input is uninstalled.
class UninstallHelper {
 public:
  // Returns true if IME environment for the current user is successfully
  // cleaned up and restored.  This function ensures following things.
  //  - 'Preload' key maintains consistency with containing at least one
  //    valid (and hopefully meaningful) IME.
  //  - Default IME is set to valid (and hopefully meaningful) IME or TIP.
  //  - Existing application is redirected to the new default IME or TIP
  //    so that a user never see any broken state caused by uninstallation.
  // Please beware that this method touches HKCU hive especially when you
  // call this method from a custom action.
  static bool RestoreUserIMEEnvironmentMain();

  // Returns true if Mozc is successfully removed from the per-user
  // settings.  Please beware that this method touches HKCU hive especially
  // when you call this method from a custom action.  If you call this
  // function from the deferred custom action which does not use
  // impersonation, it is highly recomended to set true for
  // |disable_hkcu_cache| so that HKCU points HKU/.Default as expected.
  static bool EnsureIMEIsRemovedForCurrentUser(
      bool disable_hkcu_cache);

  // Returns true if installed the list of keyboard layout and TIP is
  // retrieved in successful.
  static bool GetInstalledProfilesByLanguage(
      LANGID langid,
      vector<LayoutProfileInfo> *installed_profiles);

 private:
  // This function is the main part of RestoreUserIMEEnvironmentMain for
  // Windows XP.
  static bool RestoreUserIMEEnvironmentForXP(bool broadcast_change);

  // This function is the main part of RestoreUserIMEEnvironmentMain for
  // Windows Vista and later.
  static bool RestoreUserIMEEnvironmentForVista(bool broadcast_change);

  // Returns true if new preload layouts are successfully determined.
  static bool GetNewPreloadLayoutsForXP(
      const vector<KeyboardLayoutInfo> &preload_layouts,
      const vector<KeyboardLayoutInfo> &installed_layouts,
      vector<KeyboardLayoutInfo> *new_preloads);

  // Returns true if both new enabled profiles and new default profile are
  // successfully determined.
  static bool GetNewEnabledProfileForVista(
      const vector<LayoutProfileInfo> &current_profiles,
      const vector<LayoutProfileInfo> &installed_profiles,
      LayoutProfileInfo *current_default,
      LayoutProfileInfo *new_default,
      vector<LayoutProfileInfo> *removed_profiles);

  // Returns true if the list of keyboard layout in 'Preload' key under HKCU
  // and 'Keyboard Layouts' key under HKLM are retrieved in successful.
  // For Windows Vista and later, use GetCurrentProfilesForVista instead.
  static bool GetKeyboardLayoutsForXP(
      vector<KeyboardLayoutInfo> *preload_layouts,
      vector<KeyboardLayoutInfo> *installed_layouts);

  // Returns true if the list of keyboard layout and TIP for the current user
  // is retrieved in successful.
  static bool GetCurrentProfilesForVista(
      vector<LayoutProfileInfo> *current_profiles);

  // Returns true if the 'Preload' key for the current user is updated with
  // the specified list of keyboard layout as |preload_layouts|.
  static bool UpdatePreloadLayoutsForXP(
      const vector<KeyboardLayoutInfo> &new_preload_layouts);

  // Returns true if the list of keyboard layout and TIP for the current user
  // is updated with the specified list as |profiles_to_be_removed|.
  static bool RemoveProfilesForVista(
      const vector<LayoutProfileInfo> &profiles_to_be_removed);

  // Returns true if |layout| is set as the new default IME.  If |layout| is
  // substituted by a TIP, this function sets the underlaying TIP to default.
  static bool SetDefaultForXP(
      const KeyboardLayoutInfo &layout, bool broadcast_change);

  // Returns true if |profile| is set as the new default IME or TIP.
  static bool SetDefaultForVista(
      const LayoutProfileInfo &current_default,
      const LayoutProfileInfo &new_default, bool broadcast_change);

  // Returns a string in which the list of profile information specified in
  // |profiles| is encoded.  See input_dll.h for the format.
  // Returns an empty string if fails.
  static wstring ComposeProfileStringForVista(
      const vector<LayoutProfileInfo> &profiles);

  FRIEND_TEST(UninstallHelperTest, Issue_2950946);
  FRIEND_TEST(UninstallHelperTest, BasicCaseForVista);
  FRIEND_TEST(UninstallHelperTest, BasicCaseForWin8);
  FRIEND_TEST(UninstallHelperTest, LoadKeyboardProfilesTest);
  FRIEND_TEST(UninstallHelperTest, ComposeProfileStringForVistaTest);

  DISALLOW_COPY_AND_ASSIGN(UninstallHelper);
};
}  // namespace win32
}  // namespace mozc
#endif  // MOZC_WIN32_BASE_UNINSTALL_HELPER_H_
