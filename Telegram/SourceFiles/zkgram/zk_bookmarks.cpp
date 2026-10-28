// This file is part of ZKgram Desktop, a fork of AyuGram Desktop.
// Message bookmarks, ported from Nagram X.
//
// Licensed under GNU GPL v3. See LICENSE for details.
#include "zkgram/zk_bookmarks.h"

#include "base/unixtime.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "settings.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "window/window_session_controller.h"

#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QLocale>
#include <QtCore/QSaveFile>

namespace ZKgram {
namespace {

constexpr auto kPreviewLength = 200;

struct Bookmark {
	uint64 peer = 0;
	int64 msg = 0;
	int32 date = 0;
	QString chat;
	QString from;
	QString text;
};

std::map<uint64, std::vector<Bookmark>> All;
bool Loaded = false;

[[nodiscard]] QString BookmarksPath() {
	return cWorkingDir() + u"tdata/zkgram_bookmarks.json"_q;
}

void Load() {
	if (Loaded) {
		return;
	}
	Loaded = true;
	auto file = QFile(BookmarksPath());
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
		const auto userId = i.key().toULongLong(&ok);
		if (!ok || !i.value().isArray()) {
			continue;
		}
		auto &list = All[userId];
		for (const auto &value : i.value().toArray()) {
			const auto entry = value.toObject();
			auto bookmark = Bookmark();
			bookmark.peer = entry.value(u"peer"_q).toString().toULongLong();
			bookmark.msg = entry.value(u"msg"_q).toString().toLongLong();
			bookmark.date = entry.value(u"date"_q).toInt();
			bookmark.chat = entry.value(u"chat"_q).toString();
			bookmark.from = entry.value(u"from"_q).toString();
			bookmark.text = entry.value(u"text"_q).toString();
			if (bookmark.peer && bookmark.msg) {
				list.push_back(std::move(bookmark));
			}
		}
	}
}

void Save() {
	auto object = QJsonObject();
	for (const auto &[userId, list] : All) {
		auto array = QJsonArray();
		for (const auto &bookmark : list) {
			auto entry = QJsonObject();
			entry.insert(u"peer"_q, QString::number(bookmark.peer));
			entry.insert(u"msg"_q, QString::number(bookmark.msg));
			entry.insert(u"date"_q, bookmark.date);
			entry.insert(u"chat"_q, bookmark.chat);
			entry.insert(u"from"_q, bookmark.from);
			entry.insert(u"text"_q, bookmark.text);
			array.push_back(entry);
		}
		object.insert(QString::number(userId), array);
	}
	auto file = QSaveFile(BookmarksPath());
	if (file.open(QIODevice::WriteOnly)) {
		file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
		file.commit();
	}
}

[[nodiscard]] std::vector<Bookmark> &ListFor(
		not_null<Main::Session*> session) {
	Load();
	return All[session->userId().bare];
}

[[nodiscard]] QString PreviewText(not_null<HistoryItem*> item) {
	auto text = item->originalText().text;
	text.replace(QChar('\n'), QChar(' '));
	text = text.trimmed();
	if (text.size() > kPreviewLength) {
		text = text.mid(0, kPreviewLength) + QString::fromUtf8("\xE2\x80\xA6");
	}
	return text;
}

[[nodiscard]] QString FormatDate(int32 date) {
	return QLocale().toString(
		base::unixtime::parse(date),
		QLocale::ShortFormat);
}

void RemoveBookmark(
		not_null<Main::Session*> session,
		uint64 peer,
		int64 msg) {
	auto &list = ListFor(session);
	list.erase(ranges::remove_if(list, [&](const Bookmark &bookmark) {
		return (bookmark.peer == peer) && (bookmark.msg == msg);
	}), end(list));
	Save();
}

[[nodiscard]] const style::SettingsButton &RowStyle() {
	static const auto result = [] {
		auto copy = st::settingsButtonNoIcon;
		copy.padding.setRight(
			copy.padding.right() + st::boxTitleClose.width);
		return copy;
	}();
	return result;
}

} // namespace

bool HasBookmark(not_null<HistoryItem*> item) {
	const auto &list = ListFor(&item->history()->session());
	const auto peer = item->history()->peer->id.value;
	const auto msg = item->id.bare;
	return ranges::any_of(list, [&](const Bookmark &bookmark) {
		return (bookmark.peer == peer) && (bookmark.msg == msg);
	});
}

void ToggleBookmark(not_null<HistoryItem*> item) {
	const auto session = &item->history()->session();
	const auto peer = item->history()->peer;
	const auto added = !HasBookmark(item);
	if (added) {
		auto bookmark = Bookmark();
		bookmark.peer = peer->id.value;
		bookmark.msg = item->id.bare;
		bookmark.date = item->date();
		bookmark.chat = peer->isSelf()
			? tr::lng_saved_messages(tr::now)
			: peer->name();
		bookmark.from = item->from()->name();
		bookmark.text = PreviewText(item);
		ListFor(session).push_back(std::move(bookmark));
		Save();
	} else {
		RemoveBookmark(session, peer->id.value, item->id.bare);
	}
	if (const auto window = session->tryResolveWindow()) {
		window->showToast(added
			? tr::zk_BookmarkAdded(tr::now)
			: tr::zk_BookmarkRemoved(tr::now));
	}
}

void ClearBookmarks(not_null<Main::Session*> session) {
	ListFor(session).clear();
	Save();
}

void ShowBookmarksBox(
		not_null<Window::SessionController*> controller,
		PeerData *peer) {
	const auto session = &controller->session();
	const auto weak = base::make_weak(controller);
	const auto filterPeer = peer ? peer->id.value : uint64(0);

	auto list = std::vector<Bookmark>();
	for (const auto &bookmark : ListFor(session)) {
		if (!filterPeer || bookmark.peer == filterPeer) {
			list.push_back(bookmark);
		}
	}
	ranges::sort(list, [](const Bookmark &a, const Bookmark &b) {
		return a.date > b.date;
	});

	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::zk_Bookmarks());
		box->setWidth(st::boxWideWidth);
		box->setMaxHeight(st::boxMaxListHeight);

		if (list.empty()) {
			box->addRow(object_ptr<Ui::FlatLabel>(
				box,
				tr::zk_NoBookmarks(),
				st::boxLabel));
		}
		for (const auto &bookmark : list) {
			const auto loaded = session->data().peerLoaded(
				PeerId(bookmark.peer));
			const auto chat = loaded
				? (loaded->isSelf()
					? tr::lng_saved_messages(tr::now)
					: loaded->name())
				: bookmark.chat;
			const auto preview = bookmark.text.isEmpty()
				? tr::zk_BookmarkMedia(tr::now)
				: bookmark.text;
			const auto separator = QString::fromUtf8(" \xC2\xB7 ");
			const auto text = filterPeer
				? (FormatDate(bookmark.date) + separator + preview)
				: (chat + separator + preview);

			const auto button = box->addRow(
				object_ptr<Ui::SettingsButton>(
					box,
					rpl::producer<QString>(rpl::single(text)),
					RowStyle()),
				style::margins());
			const auto remove = Ui::CreateChild<Ui::IconButton>(
				button,
				st::boxTitleClose);
			button->sizeValue(
			) | rpl::on_next([=](QSize size) {
				remove->moveToRight(
					0,
					(size.height() - remove->height()) / 2,
					size.width());
			}, remove->lifetime());

			const auto peerValue = bookmark.peer;
			const auto msgValue = bookmark.msg;
			button->setClickedCallback([=] {
				box->closeBox();
				if (const auto strong = weak.get()) {
					strong->showPeerHistory(
						PeerId(peerValue),
						Window::SectionShow::Way::Forward,
						MsgId(msgValue));
				}
			});
			remove->setClickedCallback([=] {
				RemoveBookmark(session, peerValue, msgValue);
				box->closeBox();
				if (const auto strong = weak.get()) {
					ShowBookmarksBox(strong, peer);
				}
			});
		}
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

} // namespace ZKgram
