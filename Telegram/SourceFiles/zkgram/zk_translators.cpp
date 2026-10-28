// This file is part of ZKgram Desktop, a fork of AyuGram Desktop.
// Extra translation providers ported from Nagram X:
// AI (OpenAI-compatible LLM), DeepL and Microsoft Translator.
//
// Licensed under GNU GPL v3. See LICENSE for details.
#include "zkgram/zk_translators.h"

#include "ayu/features/translator/html_parser.h"
#include "base/random.h"
#include "zkgram/zk_settings.h"

#include <memory>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QLocale>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace ZKgram {

using namespace Ayu::Translator;

namespace {

constexpr auto kLlmTimeout = 90 * 1000;
constexpr auto kDefaultTimeout = 20 * 1000;
constexpr auto kMicrosoftTokenLifetime = crl::time(8 * 60 * 1000);

[[nodiscard]] QString LanguageName(const QString &code) {
	const auto locale = QLocale(code);
	if (locale.language() == QLocale::C) {
		return code;
	}
	const auto name = QLocale::languageToString(locale.language());
	return name.isEmpty() ? code : name;
}

[[nodiscard]] QString PrepareInput(const TextWithEntities &text) {
	return shouldWrapInHtml()
		? Html::entitiesToHtml(text)
		: text.text;
}

[[nodiscard]] TextWithEntities PrepareOutput(const QString &text) {
	return shouldWrapInHtml()
		? Html::htmlToEntities(text)
		: TextWithEntities{ text };
}

[[nodiscard]] QString StripCodeFence(QString text) {
	text = text.trimmed();
	if (text.startsWith(u"```"_q)) {
		const auto firstLineEnd = text.indexOf('\n');
		const auto fenceEnd = text.lastIndexOf(u"```"_q);
		if (firstLineEnd > 0 && fenceEnd > firstLineEnd) {
			text = text.mid(firstLineEnd + 1, fenceEnd - firstLineEnd - 1);
		}
	}
	return text.trimmed();
}

[[nodiscard]] QString PickKey(const QString &keys) {
	const auto list = keys.split(',', Qt::SkipEmptyParts);
	auto cleaned = QStringList();
	for (const auto &key : list) {
		const auto trimmed = key.trimmed();
		if (!trimmed.isEmpty()) {
			cleaned.push_back(trimmed);
		}
	}
	if (cleaned.isEmpty()) {
		return QString();
	}
	return cleaned[base::RandomIndex(int(cleaned.size()))];
}

void SetupTimeout(QPointer<QNetworkReply> reply, int timeout) {
	if (!reply) {
		return;
	}
	const auto timer = new QTimer(reply);
	timer->setSingleShot(true);
	timer->setInterval(timeout);
	QObject::connect(timer, &QTimer::timeout, reply, [reply] {
		if (reply && reply->isRunning()) {
			reply->abort();
		}
	});
	timer->start();
}

// Calls `handler` with the response body or fails on network error.
void HandleReply(
		QPointer<QNetworkReply> reply,
		Fn<void(QByteArray)> handler,
		CallbackFail onFail) {
	if (!reply) {
		return;
	}
	QObject::connect(reply, &QNetworkReply::finished, [=] {
		if (!reply) {
			return;
		}
		reply->deleteLater();
		const auto body = reply->readAll();
		if (reply->error() != QNetworkReply::NoError) {
			LOG(("ZKgram Translator: network error %1, %2"
				).arg(int(reply->error())
				).arg(QString::fromUtf8(body.left(300))));
			if (onFail) {
				onFail();
			}
			return;
		}
		handler(body);
	});
}

[[nodiscard]] QString DeeplLanguage(const QString &code) {
	const auto lower = code.toLower();
	if (lower == u"en"_q) {
		return u"EN-US"_q;
	} else if (lower == u"pt"_q) {
		return u"PT-BR"_q;
	} else if (lower.startsWith(u"zh-hant"_q) || lower == u"zh-tw"_q) {
		return u"ZH-HANT"_q;
	} else if (lower.startsWith(u"zh"_q)) {
		return u"ZH-HANS"_q;
	}
	return lower.split('-').front().toUpper();
}

[[nodiscard]] QString MicrosoftLanguage(const QString &code) {
	const auto lower = code.toLower();
	if (lower.startsWith(u"zh-hant"_q) || lower == u"zh-tw"_q) {
		return u"zh-Hant"_q;
	} else if (lower.startsWith(u"zh"_q)) {
		return u"zh-Hans"_q;
	}
	return lower.split('-').front();
}

} // namespace

LlmTranslator &LlmTranslator::instance() {
	static LlmTranslator result;
	return result;
}

LlmTranslator::LlmTranslator(QObject *parent)
: MultiThreadTranslator(parent) {
}

QPointer<QNetworkReply> LlmTranslator::startSingleTranslation(
		const MultiThreadArgs &args) {
	const auto &settings = Settings::Get();
	const auto key = PickKey(settings.llmApiKey());
	const auto &toLang = args.parsedData.toLang;
	if (args.parsedData.text.empty() || toLang.isEmpty() || key.isEmpty()) {
		if (args.onFail) {
			args.onFail();
		}
		return nullptr;
	}

	auto base = settings.llmApiUrl().trimmed();
	if (base.isEmpty()) {
		base = u"https://api.openai.com/v1"_q;
	}
	while (base.endsWith('/')) {
		base.chop(1);
	}
	const auto url = base.endsWith(u"/chat/completions"_q)
		? base
		: (base + u"/chat/completions"_q);

	const auto language = LanguageName(toLang);
	auto system = settings.llmSystemPrompt().trimmed();
	if (system.isEmpty()) {
		system = QString(
			"You are a professional translator. "
			"Translate the user's message into %1. "
			"Keep the meaning, tone, emoji, links, line breaks and "
			"all HTML tags exactly as they are. "
			"Reply with the translation only, without any comments."
		).arg(language);
	} else {
		system.replace(u"@toLang"_q, language);
	}

	auto messages = QJsonArray();
	messages.push_back(QJsonObject{
		{ u"role"_q, u"system"_q },
		{ u"content"_q, system },
	});
	messages.push_back(QJsonObject{
		{ u"role"_q, u"user"_q },
		{ u"content"_q, PrepareInput(args.parsedData.text) },
	});
	auto body = QJsonObject{
		{ u"model"_q, settings.llmModel().trimmed() },
		{ u"messages"_q, messages },
		{ u"temperature"_q, settings.llmTemperature() / 100. },
		{ u"stream"_q, false },
	};

	auto request = QNetworkRequest(QUrl(url));
	request.setHeader(
		QNetworkRequest::ContentTypeHeader,
		u"application/json"_q);
	request.setRawHeader("Authorization", "Bearer " + key.toUtf8());

	const auto reply = QPointer<QNetworkReply>(_nam.post(
		request,
		QJsonDocument(body).toJson(QJsonDocument::Compact)));
	SetupTimeout(reply, kLlmTimeout);

	const auto onSuccess = args.onSuccess;
	const auto onFail = args.onFail;
	HandleReply(reply, [=](QByteArray data) {
		auto ok = false;
		const auto content = parseJsonPath(
			data,
			u"choices[0].message.content"_q,
			&ok);
		if (!ok || content.trimmed().isEmpty()) {
			if (onFail) {
				onFail();
			}
			return;
		}
		if (onSuccess) {
			onSuccess(PrepareOutput(StripCodeFence(content)));
		}
	}, onFail);
	return reply;
}

DeeplTranslator &DeeplTranslator::instance() {
	static DeeplTranslator result;
	return result;
}

DeeplTranslator::DeeplTranslator(QObject *parent)
: MultiThreadTranslator(parent) {
}

QPointer<QNetworkReply> DeeplTranslator::startSingleTranslation(
		const MultiThreadArgs &args) {
	const auto key = PickKey(Settings::Get().deeplApiKey());
	const auto &toLang = args.parsedData.toLang;
	if (args.parsedData.text.empty() || toLang.isEmpty() || key.isEmpty()) {
		if (args.onFail) {
			args.onFail();
		}
		return nullptr;
	}
	const auto url = key.endsWith(u":fx"_q)
		? u"https://api-free.deepl.com/v2/translate"_q
		: u"https://api.deepl.com/v2/translate"_q;

	auto texts = QJsonArray();
	texts.push_back(PrepareInput(args.parsedData.text));
	auto body = QJsonObject{
		{ u"text"_q, texts },
		{ u"target_lang"_q, DeeplLanguage(toLang) },
	};
	if (shouldWrapInHtml()) {
		body.insert(u"tag_handling"_q, u"html"_q);
	}

	auto request = QNetworkRequest(QUrl(url));
	request.setHeader(
		QNetworkRequest::ContentTypeHeader,
		u"application/json"_q);
	request.setRawHeader(
		"Authorization",
		"DeepL-Auth-Key " + key.toUtf8());

	const auto reply = QPointer<QNetworkReply>(_nam.post(
		request,
		QJsonDocument(body).toJson(QJsonDocument::Compact)));
	SetupTimeout(reply, kDefaultTimeout);

	const auto onSuccess = args.onSuccess;
	const auto onFail = args.onFail;
	HandleReply(reply, [=](QByteArray data) {
		auto ok = false;
		const auto text = parseJsonPath(
			data,
			u"translations[0].text"_q,
			&ok);
		if (!ok) {
			if (onFail) {
				onFail();
			}
			return;
		}
		if (onSuccess) {
			onSuccess(PrepareOutput(text));
		}
	}, onFail);
	return reply;
}

MicrosoftTranslator &MicrosoftTranslator::instance() {
	static MicrosoftTranslator result;
	return result;
}

MicrosoftTranslator::MicrosoftTranslator(QObject *parent)
: MultiThreadTranslator(parent) {
}

QPointer<QNetworkReply> MicrosoftTranslator::startSingleTranslation(
		const MultiThreadArgs &args) {
	if (args.parsedData.text.empty() || args.parsedData.toLang.isEmpty()) {
		if (args.onFail) {
			args.onFail();
		}
		return nullptr;
	}
	const auto tokenValid = !_token.isEmpty()
		&& (crl::now() - _tokenReceived) < kMicrosoftTokenLifetime;
	if (tokenValid) {
		return translateWithToken(args);
	}

	auto request = QNetworkRequest(
		QUrl(u"https://edge.microsoft.com/translate/auth"_q));
	request.setHeader(
		QNetworkRequest::UserAgentHeader,
		randomDesktopUserAgent());
	const auto reply = QPointer<QNetworkReply>(_nam.get(request));
	SetupTimeout(reply, kDefaultTimeout);

	const auto copy = args;
	HandleReply(reply, [=, this](QByteArray data) {
		const auto token = QString::fromUtf8(data).trimmed();
		if (token.isEmpty()) {
			if (copy.onFail) {
				copy.onFail();
			}
			return;
		}
		_token = token;
		_tokenReceived = crl::now();
		translateWithToken(copy);
	}, args.onFail);
	return reply;
}

QPointer<QNetworkReply> MicrosoftTranslator::translateWithToken(
		const MultiThreadArgs &args) {
	auto url = QUrl(
		u"https://api-edge.cognitive.microsofttranslator.com/translate"_q);
	auto query = QUrlQuery();
	query.addQueryItem(u"api-version"_q, u"3.0"_q);
	query.addQueryItem(u"to"_q, MicrosoftLanguage(args.parsedData.toLang));
	if (shouldWrapInHtml()) {
		query.addQueryItem(u"textType"_q, u"html"_q);
	}
	url.setQuery(query);

	auto texts = QJsonArray();
	texts.push_back(QJsonObject{
		{ u"Text"_q, PrepareInput(args.parsedData.text) },
	});

	auto request = QNetworkRequest(url);
	request.setHeader(
		QNetworkRequest::ContentTypeHeader,
		u"application/json"_q);
	request.setRawHeader("Authorization", "Bearer " + _token.toUtf8());

	const auto reply = QPointer<QNetworkReply>(_nam.post(
		request,
		QJsonDocument(texts).toJson(QJsonDocument::Compact)));
	SetupTimeout(reply, kDefaultTimeout);

	const auto onSuccess = args.onSuccess;
	const auto onFail = args.onFail;
	HandleReply(reply, [=, this](QByteArray data) {
		auto ok = false;
		const auto text = parseJsonPath(
			data,
			u"[0].translations[0].text"_q,
			&ok);
		if (!ok) {
			_token = QString();
			if (onFail) {
				onFail();
			}
			return;
		}
		if (onSuccess) {
			onSuccess(PrepareOutput(text));
		}
	}, onFail);
	return reply;
}

} // namespace ZKgram
