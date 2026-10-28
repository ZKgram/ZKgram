// This file is part of ZKgram Desktop, a fork of AyuGram Desktop.
// Message bookmarks, ported from Nagram X.
//
// Licensed under GNU GPL v3. See LICENSE for details.
#pragma once

class HistoryItem;
class PeerData;

namespace Main {
class Session;
} // namespace Main

namespace Window {
class SessionController;
} // namespace Window

namespace ZKgram {

[[nodiscard]] bool HasBookmark(not_null<HistoryItem*> item);
void ToggleBookmark(not_null<HistoryItem*> item);

// Shows bookmarks of the given chat, or all bookmarks if peer is nullptr.
void ShowBookmarksBox(
	not_null<Window::SessionController*> controller,
	PeerData *peer);

void ClearBookmarks(not_null<Main::Session*> session);

} // namespace ZKgram
