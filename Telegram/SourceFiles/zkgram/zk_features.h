// This file is part of ZKgram Desktop, a fork of AyuGram Desktop
// (which is a fork of Telegram Desktop). Features in this folder are
// ported from Nagram X (https://github.com/risin42/NagramX).
//
// Licensed under GNU GPL v3. See LICENSE for details.
#pragma once

#include "data/data_peer_id.h"
#include "ui/widgets/menu/menu_add_action_callback.h"

#include <QtCore/QVariant>

class HistoryItem;
class PeerData;
class UserData;

namespace Data {
class Thread;
} // namespace Data

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class PopupMenu;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace ZKgram {

void InitFeatures();

// Confirmations.
// Each returns true if the action was intercepted and will be performed
// later by calling `proceed` after the user confirms it.
[[nodiscard]] bool InterceptOutgoingCall(
	not_null<UserData*> user,
	Fn<void()> proceed);
[[nodiscard]] bool InterceptBotCommand(
	const QString &command,
	Window::SessionController *controller,
	Fn<void()> proceed);
[[nodiscard]] bool InterceptUrlClick(
	const QString &url,
	const QVariant &context);

// While alive, link clicks are opened without the ZKgram confirmation
// (used when the user already confirmed the link in another box).
class UrlConfirmationBypass final {
public:
	UrlConfirmationBypass();
	~UrlConfirmationBypass();

	UrlConfirmationBypass(const UrlConfirmationBypass &) = delete;
	UrlConfirmationBypass &operator=(const UrlConfirmationBypass &) = delete;

private:
	bool _was = false;

};

// Display names (name order and local name overrides).
[[nodiscard]] QString AdjustUserName(
	PeerId id,
	const QString &firstName,
	const QString &lastName,
	const QString &fullName);
[[nodiscard]] QString AdjustPeerName(PeerId id, const QString &serverName);
[[nodiscard]] QString LocalName(PeerId id);
void SetLocalName(not_null<PeerData*> peer, const QString &name);
void ResetAllLocalNames(not_null<Main::Session*> session);

// Delete chat box: "Also delete for ..." default.
[[nodiscard]] bool DeleteChatForBothSidesByDefault();

// Menus.
void AddPeerActions(
	PeerData *peer,
	Data::Thread *thread,
	not_null<Window::SessionController*> controller,
	const Ui::Menu::MenuCallback &addAction);
void AddMessageActions(
	not_null<Ui::PopupMenu*> menu,
	HistoryItem *item);

// Tools.
void ShowGoToMessageBox(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer);
void ShowLocalNameBox(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer);
void UnblockAll(
	not_null<Window::SessionController*> controller,
	bool onlyDeleted);
void ExportSettings(not_null<Window::SessionController*> controller);
void ImportSettings(not_null<Window::SessionController*> controller);

} // namespace ZKgram
