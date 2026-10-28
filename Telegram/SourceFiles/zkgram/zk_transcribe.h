// This file is part of ZKgram Desktop, a fork of AyuGram Desktop.
// AI voice transcription through an OpenAI-compatible API (Whisper),
// ported from Nagram X.
//
// Licensed under GNU GPL v3. See LICENSE for details.
#pragma once

class HistoryItem;

namespace ZKgram {

[[nodiscard]] bool TranscribeEnabled();

// Downloads the voice / round video of the item (if needed) and sends it
// to the configured speech-to-text API. Exactly one of the callbacks
// is called.
void Transcribe(
	not_null<HistoryItem*> item,
	Fn<void(QString text)> done,
	Fn<void(QString error)> fail);

} // namespace ZKgram
