// This file is part of ZKgram Desktop, a fork of AyuGram Desktop.
// AI voice transcription through an OpenAI-compatible API (Whisper),
// ported from Nagram X.
//
// Licensed under GNU GPL v3. See LICENSE for details.
#include "zkgram/zk_transcribe.h"

#include "base/call_delayed.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_media_types.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "zkgram/zk_settings.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtNetwork/QHttpMultiPart>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace ZKgram {
namespace {

constexpr auto kDownloadCheckDelay = crl::time(500);
constexpr auto kDownloadMaxChecks = 240; // 2 minutes.
constexpr auto kRequestTimeout = 120 * 1000;

struct Pending {
	not_null<Main::Session*> session;
	FullMsgId itemId;
	not_null<DocumentData*> document;
	std::shared_ptr<Data::DocumentMedia> media;
	Fn<void(QString)> done;
	Fn<void(QString)> fail;
	int checks = 0;
};

[[nodiscard]] QNetworkAccessManager &Network() {
	static auto result = QNetworkAccessManager();
	return result;
}

[[nodiscard]] QByteArray ReadContent(const std::shared_ptr<Pending> &state) {
	auto bytes = state->media->bytes();
	if (!bytes.isEmpty()) {
		return bytes;
	}
	const auto path = state->document->filepath(true);
	if (!path.isEmpty()) {
		auto file = QFile(path);
		if (file.open(QIODevice::ReadOnly)) {
			return file.readAll();
		}
	}
	return QByteArray();
}

void Upload(
		const std::shared_ptr<Pending> &state,
		const QByteArray &content) {
	const auto &settings = Settings::Get();
	auto base = settings.transcribeApiUrl().trimmed();
	if (base.isEmpty()) {
		base = u"https://api.groq.com/openai/v1"_q;
	}
	while (base.endsWith('/')) {
		base.chop(1);
	}
	const auto url = base.endsWith(u"/audio/transcriptions"_q)
		? base
		: (base + u"/audio/transcriptions"_q);
	const auto key = settings.transcribeApiKey().trimmed();
	const auto model = settings.transcribeModel().trimmed();
	const auto language = settings.transcribeLanguage().trimmed();

	const auto video = state->document->isVideoMessage();
	const auto fileName = video ? u"video.mp4"_q : u"voice.ogg"_q;
	const auto mime = video ? u"video/mp4"_q : u"audio/ogg"_q;

	const auto multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
	const auto addField = [&](const QString &name, const QString &value) {
		auto part = QHttpPart();
		part.setHeader(
			QNetworkRequest::ContentDispositionHeader,
			QVariant(u"form-data; name=\"%1\""_q.arg(name)));
		part.setBody(value.toUtf8());
		multi->append(part);
	};
	{
		auto part = QHttpPart();
		part.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(mime));
		part.setHeader(
			QNetworkRequest::ContentDispositionHeader,
			QVariant(u"form-data; name=\"file\"; filename=\"%1\""_q
				.arg(fileName)));
		part.setBody(content);
		multi->append(part);
	}
	addField(
		u"model"_q,
		model.isEmpty() ? u"whisper-large-v3-turbo"_q : model);
	addField(u"response_format"_q, u"json"_q);
	if (!language.isEmpty()) {
		addField(u"language"_q, language);
	}

	auto request = QNetworkRequest(QUrl(url));
	request.setRawHeader("Authorization", "Bearer " + key.toUtf8());

	const auto reply = QPointer<QNetworkReply>(
		Network().post(request, multi));
	multi->setParent(reply.data());

	const auto timer = new QTimer(reply.data());
	timer->setSingleShot(true);
	timer->setInterval(kRequestTimeout);
	QObject::connect(timer, &QTimer::timeout, reply.data(), [reply] {
		if (reply && reply->isRunning()) {
			reply->abort();
		}
	});
	timer->start();

	QObject::connect(reply.data(), &QNetworkReply::finished, [=] {
		if (!reply) {
			return;
		}
		reply->deleteLater();
		const auto body = reply->readAll();
		if (reply->error() != QNetworkReply::NoError) {
			auto error = QString::fromUtf8(body).trimmed();
			const auto document = QJsonDocument::fromJson(body);
			if (document.isObject()) {
				const auto object = document.object();
				const auto message = object.value(u"error"_q).toObject()
					.value(u"message"_q).toString();
				if (!message.isEmpty()) {
					error = message;
				}
			}
			if (error.isEmpty()) {
				error = reply->errorString();
			}
			state->fail(error.left(200));
			return;
		}
		const auto document = QJsonDocument::fromJson(body);
		const auto text = document.isObject()
			? document.object().value(u"text"_q).toString().trimmed()
			: QString::fromUtf8(body).trimmed();
		state->done(text);
	});
}

void Proceed(const std::shared_ptr<Pending> &state) {
	const auto content = ReadContent(state);
	if (!content.isEmpty()) {
		Upload(state, content);
		return;
	}
	if (++state->checks > kDownloadMaxChecks) {
		state->fail(u"download timeout"_q);
		return;
	}
	if (state->checks == 1 && !state->document->loading()) {
		state->document->save(state->itemId, QString());
	}
	base::call_delayed(kDownloadCheckDelay, state->session, [=] {
		Proceed(state);
	});
}

} // namespace

bool TranscribeEnabled() {
	const auto &settings = Settings::Get();
	return settings.transcribeEnabled()
		&& !settings.transcribeApiKey().trimmed().isEmpty();
}

void Transcribe(
		not_null<HistoryItem*> item,
		Fn<void(QString text)> done,
		Fn<void(QString error)> fail) {
	const auto media = item->media();
	const auto document = media ? media->document() : nullptr;
	if (!document
		|| !(document->isVoiceMessage() || document->isVideoMessage())) {
		fail(u"unsupported media"_q);
		return;
	}
	const auto state = std::make_shared<Pending>(Pending{
		.session = &item->history()->session(),
		.itemId = item->fullId(),
		.document = document,
		.media = document->createMediaView(),
		.done = std::move(done),
		.fail = std::move(fail),
	});
	Proceed(state);
}

} // namespace ZKgram
