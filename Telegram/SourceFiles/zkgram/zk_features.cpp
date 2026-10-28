// This file is part of ZKgram Desktop, a fork of AyuGram Desktop
// (which is a fork of Telegram Desktop). Features in this folder are
// ported from Nagram X (https://github.com/risin42/NagramX).
//
// Licensed under GNU GPL v3. See LICENSE for details.
#include "zkgram/zk_features.h"

#include "zkgram/zk_bookmarks.h"
#include "zkgram/zk_settings.h"

#include "apiwrap.h"
#include "api/api_blocked_peers.h"
#include "ayu/ayu_settings.h"
#include "ayu/ui/boxes/edit_mark_box.h"
#include "base/call_delayed.h"
#include "base/qt/qt_key_modifiers.h"
#include "boxes/abstract_box.h"
#include "core/application.h"
#include "core/click_handler_types.h"
#include "core/file_utilities.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_thread.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "settings.h"
#include "styles/style_menu_icons.h"
#include "ui/boxes/confirm_box.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/menu/menu_add_action_callback_factory.h"
#include "window/window_session_controller.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>

namespace ZKgram {
namespace {

constexpr auto kBlockedPage = 100;
constexpr auto kUnblockDelay = crl::time(200);

bool UrlBypassActive = false;

// Local name overrides: peerId -> name.
base::flat_map<uint64, QString> LocalNames;
// Last name received from the server for chats and channels,
// so that a local override can be removed without a reload.
base::flat_map<uint64, QString> ServerNames;
bool LocalNamesLoaded = false;

[[nodiscard]] QString LocalNamesPath() {
	return cWorkingDir() + u"tdata/zkgram_local_names.json"_q;
}

void LoadLocalNames() {
	if (LocalNamesLoaded) {
		return;
	}
	LocalNamesLoaded = true;
	auto file = QFile(LocalNamesPath());
	if (!file.open(QIODevice::ReadOnly)) {
		return;
	}
	const auto document = QJsonDocument::fromJson(file.readAll());
	file.close();
	if (!document.isObject()) {
		return;
	}
	const auto object = document.object();
	for (auto i = object.begin(); i != object.end(); ++i) {
		auto ok = false;
		const auto id = i.key().toULongLong(&ok);
		const auto name = i.value().toString().trimmed();
		if (ok && !name.isEmpty()) {
			LocalNames[id] = name;
		}
	}
}

void SaveLocalNames() {
	auto object = QJsonObject();
	for (const auto &[id, name] : LocalNames) {
		object.insert(QString::number(id), name);
	}
	auto file = QSaveFile(LocalNamesPath());
	if (file.open(QIODevice::WriteOnly)) {
		file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
		file.commit();
	}
}

void ReapplyName(not_null<PeerData*> peer) {
	if (const auto user = peer->asUser()) {
		user->setName(
			user->firstName,
			user->lastName,
			user->nameOrPhone,
			user->editableUsername());
	} else if (const auto channel = peer->asChannel()) {
		const auto i = ServerNames.find(peer->id.value);
		channel->setName(
			(i != end(ServerNames)) ? i->second : channel->name(),
			channel->editableUsername());
	} else if (const auto chat = peer->asChat()) {
		const auto i = ServerNames.find(peer->id.value);
		chat->setName((i != end(ServerNames)) ? i->second : chat->name());
	}
}

void ShowBox(
		Window::SessionController *controller,
		object_ptr<Ui::BoxContent> box) {
	if (controller) {
		controller->show(std::move(box));
	} else {
		Ui::show(std::move(box));
	}
}

struct UnblockState {
	std::vector<PeerId> peers;
	int index = 0;
	int unblocked = 0;
	bool onlyDeleted = false;
};

void UnblockNext(
		base::weak_ptr<Window::SessionController> weak,
		not_null<Main::Session*> session,
		std::shared_ptr<UnblockState> state) {
	const auto finish = [=] {
		if (const auto controller = weak.get()) {
			controller->showToast(tr::zk_UnblockDone(
				tr::now,
				lt_item,
				QString::number(state->unblocked)));
		}
	};
	while (state->index < int(state->peers.size())) {
		const auto id = state->peers[state->index++];
		const auto peer = session->data().peerLoaded(id);
		if (!peer) {
			continue;
		}
		if (state->onlyDeleted) {
			const auto user = peer->asUser();
			if (!user || !user->isInaccessible()) {
				continue;
			}
		}
		session->api().blockedPeers().unblock(peer, [=](bool success) {
			if (success) {
				++state->unblocked;
			}
			base::call_delayed(kUnblockDelay, session, [=] {
				UnblockNext(weak, session, state);
			});
		}, true);
		return;
	}
	finish();
}

void LoadBlockedPage(
		not_null<Main::Session*> session,
		int offset,
		std::shared_ptr<UnblockState> state,
		Fn<void()> done) {
	session->api().request(MTPcontacts_GetBlocked(
		MTP_flags(0),
		MTP_int(offset),
		MTP_int(kBlockedPage)
	)).done([=](const MTPcontacts_Blocked &result) {
		auto received = 0;
		auto total = -1;
		const auto collect = [&](const QVector<MTPPeerBlocked> &list) {
			for (const auto &blocked : list) {
				blocked.match([&](const MTPDpeerBlocked &data) {
					state->peers.push_back(peerFromMTP(data.vpeer_id()));
				});
				++received;
			}
		};
		result.match([&](const MTPDcontacts_blockedSlice &data) {
			session->data().processUsers(data.vusers());
			session->data().processChats(data.vchats());
			total = data.vcount().v;
			collect(data.vblocked().v);
		}, [&](const MTPDcontacts_blocked &data) {
			session->data().processUsers(data.vusers());
			session->data().processChats(data.vchats());
			collect(data.vblocked().v);
		});
		const auto next = offset + received;
		if (total > 0 && received > 0 && next < total) {
			LoadBlockedPage(session, next, state, done);
		} else {
			done();
		}
	}).fail([=] {
		done();
	}).send();
}

} // namespace

void InitFeatures() {
	LoadLocalNames();
}

UrlConfirmationBypass::UrlConfirmationBypass()
: _was(UrlBypassActive) {
	UrlBypassActive = true;
}

UrlConfirmationBypass::~UrlConfirmationBypass() {
	UrlBypassActive = _was;
}

bool InterceptOutgoingCall(
		not_null<UserData*> user,
		Fn<void()> proceed) {
	if (!Settings::Get().askBeforeCall()) {
		return false;
	}
	Ui::show(Ui::MakeConfirmBox({
		.text = tr::zk_ConfirmCallText(tr::now, lt_user, user->name()),
		.confirmed = [=](Fn<void()> close) {
			close();
			proceed();
		},
		.confirmText = tr::lng_profile_action_short_call(),
	}));
	return true;
}

bool InterceptBotCommand(
		const QString &command,
		Window::SessionController *controller,
		Fn<void()> proceed) {
	if (!Settings::Get().askBeforeBotCommand()) {
		return false;
	}
	ShowBox(controller, Ui::MakeConfirmBox({
		.text = tr::zk_ConfirmBotCommandText(tr::now, lt_item, command),
		.confirmed = [=](Fn<void()> close) {
			close();
			proceed();
		},
		.confirmText = tr::lng_send_button(),
	}));
	return true;
}

bool InterceptUrlClick(const QString &url, const QVariant &context) {
	if (UrlBypassActive
		|| !Settings::Get().confirmAllLinks()
		|| url.isEmpty()
		|| url.startsWith(u"internal:"_q, Qt::CaseInsensitive)
		|| url.startsWith(u"mailto:"_q, Qt::CaseInsensitive)
		|| base::IsCtrlPressed()) {
		return false;
	}
	const auto my = context.value<ClickHandlerContext>();
	auto box = Ui::MakeConfirmBox({
		.text = tr::lng_open_this_link(tr::now) + u"\n\n"_q + url,
		.confirmed = [=](Fn<void()> close) {
			close();
			const auto bypass = UrlConfirmationBypass();
			UrlClickHandler::Open(url, context);
		},
		.confirmText = tr::lng_open_link(),
	});
	if (const auto controller = my.sessionWindow.get()) {
		controller->show(std::move(box));
	} else if (my.show) {
		my.show->showBox(std::move(box));
	} else {
		Ui::show(std::move(box));
	}
	return true;
}

QString AdjustUserName(
		PeerId id,
		const QString &firstName,
		const QString &lastName,
		const QString &fullName) {
	LoadLocalNames();
	const auto i = LocalNames.find(id.value);
	if (i != end(LocalNames)) {
		return i->second;
	}
	if (Settings::Get().resolvedNameOrder() == NameOrder::LastFirst
		&& !firstName.trimmed().isEmpty()
		&& !lastName.trimmed().isEmpty()) {
		return lastName + ' ' + firstName;
	}
	return fullName;
}

QString AdjustPeerName(PeerId id, const QString &serverName) {
	LoadLocalNames();
	const auto i = LocalNames.find(id.value);
	if (i == end(LocalNames)) {
		ServerNames[id.value] = serverName;
		return serverName;
	}
	if (serverName != i->second) {
		ServerNames[id.value] = serverName;
	}
	return i->second;
}

QString LocalName(PeerId id) {
	LoadLocalNames();
	const auto i = LocalNames.find(id.value);
	return (i != end(LocalNames)) ? i->second : QString();
}

void SetLocalName(not_null<PeerData*> peer, const QString &name) {
	LoadLocalNames();
	const auto trimmed = name.trimmed();
	if (trimmed.isEmpty()) {
		LocalNames.remove(peer->id.value);
	} else {
		LocalNames[peer->id.value] = trimmed;
	}
	SaveLocalNames();
	ReapplyName(peer);
}

void ResetAllLocalNames(not_null<Main::Session*> session) {
	LoadLocalNames();
	const auto ids = base::take(LocalNames);
	SaveLocalNames();
	for (const auto &[id, name] : ids) {
		if (const auto peer = session->data().peerLoaded(PeerId(id))) {
			ReapplyName(peer);
		}
	}
}

bool DeleteChatForBothSidesByDefault() {
	return Settings::Get().deleteChatForBothSides();
}

void AddPeerActions(
		PeerData *peer,
		Data::Thread *thread,
		not_null<Window::SessionController*> controller,
		const Ui::Menu::MenuCallback &addAction) {
	if (!peer) {
		return;
	}
	const auto weak = base::make_weak(controller);
	addAction(Ui::Menu::MenuCallback::Args{
		.text = u"ZKgram"_q,
		.handler = nullptr,
		.icon = &st::menuIconCustomize,
		.fillSubmenu = [=](not_null<Ui::PopupMenu*> menu) {
			const auto add = Ui::Menu::CreateAddActionCallback(menu);
			add(tr::zk_Bookmarks(tr::now), [=] {
				if (const auto strong = weak.get()) {
					ShowBookmarksBox(strong, peer);
				}
			}, &st::menuIconFave);
			add(tr::zk_GoToMessage(tr::now), [=] {
				if (const auto strong = weak.get()) {
					ShowGoToMessageBox(strong, peer);
				}
			}, &st::menuIconShowInChat);
			if (!peer->isSelf()) {
				add(tr::zk_SetLocalName(tr::now), [=] {
					if (const auto strong = weak.get()) {
						ShowLocalNameBox(strong, peer);
					}
				}, &st::menuIconEdit);
				if (!LocalName(peer->id).isEmpty()) {
					add(tr::zk_ResetLocalName(tr::now), [=] {
						SetLocalName(peer, QString());
					}, &st::menuIconRestore);
				}
			}
		},
	});
}

void AddMessageActions(
		not_null<Ui::PopupMenu*> menu,
		HistoryItem *item) {
	if (!item || !item->isRegular()) {
		return;
	}
	const auto itemId = item->fullId();
	const auto session = &item->history()->session();
	const auto has = HasBookmark(item);
	menu->addAction(
		has ? tr::zk_RemoveBookmark(tr::now) : tr::zk_AddBookmark(tr::now),
		[=] {
			if (const auto item = session->data().message(itemId)) {
				ToggleBookmark(item);
			}
		},
		has ? &st::menuIconUnfave : &st::menuIconFave);
}

void ShowGoToMessageBox(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer) {
	const auto weak = base::make_weak(controller);
	const auto peerId = peer->id;
	controller->show(Box<EditMarkBox>(
		tr::zk_MessageIdPlaceholder(),
		QString(),
		QString(),
		[=](const QString &value) {
			auto ok = false;
			const auto id = value.trimmed().toLongLong(&ok);
			if (!ok || id <= 0) {
				return;
			}
			if (const auto strong = weak.get()) {
				strong->showPeerHistory(
					peerId,
					Window::SectionShow::Way::Forward,
					MsgId(id));
			}
		}));
}

void ShowLocalNameBox(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer) {
	const auto current = LocalName(peer->id);
	controller->show(Box<EditMarkBox>(
		tr::zk_LocalNamePlaceholder(),
		current.isEmpty() ? peer->name() : current,
		QString(),
		[=](const QString &value) {
			SetLocalName(peer, value);
		}));
}

void UnblockAll(
		not_null<Window::SessionController*> controller,
		bool onlyDeleted) {
	const auto weak = base::make_weak(controller);
	const auto session = &controller->session();
	controller->show(Ui::MakeConfirmBox({
		.text = (onlyDeleted
			? tr::zk_UnblockDeletedConfirm(tr::now)
			: tr::zk_UnblockAllConfirm(tr::now)),
		.confirmed = [=](Fn<void()> close) {
			close();
			if (const auto strong = weak.get()) {
				strong->showToast(tr::zk_UnblockInProgress(tr::now));
			}
			const auto state = std::make_shared<UnblockState>();
			state->onlyDeleted = onlyDeleted;
			LoadBlockedPage(session, 0, state, [=] {
				UnblockNext(weak, session, state);
			});
		},
		.confirmText = tr::lng_blocked_list_unblock(),
	}));
}

void ExportSettings(not_null<Window::SessionController*> controller) {
	const auto weak = base::make_weak(controller);
	FileDialog::GetWritePath(
		Core::App().getFileDialogParent(),
		tr::zk_ExportSettings(tr::now),
		u"JSON (*.json)"_q,
		u"zkgram_settings.json"_q,
		[=](QString &&path) {
			if (path.isEmpty()) {
				return;
			}
			auto object = QJsonObject();
			object.insert(u"zkgram"_q, Settings::Get().toJson());
			auto ayu = QFile(cWorkingDir() + u"tdata/ayu_settings.json"_q);
			if (ayu.open(QIODevice::ReadOnly)) {
				const auto document = QJsonDocument::fromJson(ayu.readAll());
				if (document.isObject()) {
					object.insert(u"ayugram"_q, document.object());
				}
			}
			auto file = QSaveFile(path);
			if (file.open(QIODevice::WriteOnly)) {
				file.write(QJsonDocument(object).toJson(
					QJsonDocument::Indented));
				if (file.commit()) {
					if (const auto strong = weak.get()) {
						strong->showToast(tr::zk_SettingsExported(tr::now));
					}
				}
			}
		});
}

void ImportSettings(not_null<Window::SessionController*> controller) {
	const auto weak = base::make_weak(controller);
	FileDialog::GetOpenPath(
		Core::App().getFileDialogParent(),
		tr::zk_ImportSettings(tr::now),
		u"JSON (*.json)"_q,
		[=](FileDialog::OpenResult &&result) {
			const auto fail = [=] {
				if (const auto strong = weak.get()) {
					strong->showToast(tr::zk_SettingsImportFailed(tr::now));
				}
			};
			if (result.paths.isEmpty()) {
				return;
			}
			auto file = QFile(result.paths.front());
			if (!file.open(QIODevice::ReadOnly)) {
				fail();
				return;
			}
			const auto document = QJsonDocument::fromJson(file.readAll());
			file.close();
			if (!document.isObject()) {
				fail();
				return;
			}
			const auto object = document.object();
			const auto zk = object.value(u"zkgram"_q);
			const auto ayu = object.value(u"ayugram"_q);
			if (!zk.isObject() && !ayu.isObject()) {
				fail();
				return;
			}
			if (zk.isObject()) {
				Settings::Get().fromJson(zk.toObject());
				Settings::Save();
			}
			if (ayu.isObject()) {
				auto out = QSaveFile(
					cWorkingDir() + u"tdata/ayu_settings.json"_q);
				if (out.open(QIODevice::WriteOnly)) {
					out.write(QJsonDocument(ayu.toObject()).toJson(
						QJsonDocument::Indented));
					out.commit();
				}
			}
			if (const auto strong = weak.get()) {
				strong->show(Ui::MakeConfirmBox({
					.text = tr::zk_SettingsImported(tr::now),
					.confirmed = [] { Core::Restart(); },
					.confirmText = tr::lng_settings_restart_now(),
				}));
			}
		});
}

} // namespace ZKgram
