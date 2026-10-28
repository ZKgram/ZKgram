// This file is part of ZKgram Desktop, a fork of AyuGram Desktop.
//
// Licensed under GNU GPL v3. See LICENSE for details.
#pragma once

namespace ZKgram {

// Applies ZKgram translations (zk_* keys) for the current language
// and replaces AyuGram branding in translated AyuGram strings.
// Safe to call many times, e.g. after every language pack update.
void ApplyTranslations();

} // namespace ZKgram
