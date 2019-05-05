// Copyright 2010 Google Inc. All Rights Reserved.

#ifndef MOZC_UNIX_IBUS_MAIN_H_
#define MOZC_UNIX_IBUS_MAIN_H_
namespace {
const char kComponentVersion[] = "0.0.0.0";
const char kComponentName[] = "com.google.IBus.Mozc";
const char kComponentLicense[] = "New BSD";
const char kComponentExec[] = "/usr/lib/ibus-mozc/ibus-engine-mozc --ibus";
const char kComponentTextdomain[] = "ibus-mozc";
const char kComponentAuthor[] = "Google Inc.";
const char kComponentHomepage[] = "https://github.com/google/mozc";
const char kComponentDescription[] = "Mozc Component";
const char kEngineDescription[] = "Mozc (Japanese Input Method)";
const char kEngineLanguage[] = "ja";
const char kEngineSymbol[] = "&#x3042;";
const char kEngineRank[] = "80";
const char kEngineIcon_prop_key[] = "InputMode";
const char kEngineIcon[] = "/usr/share/ibus-mozc/product_icon.png";
const char* kEngineLayoutArray[] = {
"default",
};
const char* kEngineNameArray[] = {
"mozc-jp",
};
const char* kEngineLongnameArray[] = {
"Mozc",
};
const size_t kEngineArrayLen = 1;
}  // namespace
#endif  // MOZC_UNIX_IBUS_MAIN_H_
