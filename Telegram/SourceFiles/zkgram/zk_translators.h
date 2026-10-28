// This file is part of ZKgram Desktop, a fork of AyuGram Desktop.
// Extra translation providers ported from Nagram X:
// AI (OpenAI-compatible LLM), DeepL and Microsoft Translator.
//
// Licensed under GNU GPL v3. See LICENSE for details.
#pragma once

#include "ayu/features/translator/implementations/base.h"

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtNetwork/QNetworkAccessManager>

namespace ZKgram {

class LlmTranslator final : public Ayu::Translator::MultiThreadTranslator {
	Q_OBJECT

public:
	static LlmTranslator &instance();

	[[nodiscard]] int getConcurrencyLimit() const override { return 2; }
	[[nodiscard]] int getMaxRetries() const override { return 2; }

	[[nodiscard]] QPointer<QNetworkReply> startSingleTranslation(
		const Ayu::Translator::MultiThreadArgs &args) override;

private:
	explicit LlmTranslator(QObject *parent = nullptr);

	QNetworkAccessManager _nam;

};

class DeeplTranslator final : public Ayu::Translator::MultiThreadTranslator {
	Q_OBJECT

public:
	static DeeplTranslator &instance();

	[[nodiscard]] int getConcurrencyLimit() const override { return 2; }

	[[nodiscard]] QPointer<QNetworkReply> startSingleTranslation(
		const Ayu::Translator::MultiThreadArgs &args) override;

private:
	explicit DeeplTranslator(QObject *parent = nullptr);

	QNetworkAccessManager _nam;

};

class MicrosoftTranslator final
	: public Ayu::Translator::MultiThreadTranslator {
	Q_OBJECT

public:
	static MicrosoftTranslator &instance();

	[[nodiscard]] int getConcurrencyLimit() const override { return 3; }

	[[nodiscard]] QPointer<QNetworkReply> startSingleTranslation(
		const Ayu::Translator::MultiThreadArgs &args) override;

private:
	explicit MicrosoftTranslator(QObject *parent = nullptr);

	[[nodiscard]] QPointer<QNetworkReply> translateWithToken(
		const Ayu::Translator::MultiThreadArgs &args);

	QNetworkAccessManager _nam;
	QString _token;
	crl::time _tokenReceived = 0;

};

} // namespace ZKgram
