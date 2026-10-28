// This file is part of ZKgram Desktop, a fork of AyuGram Desktop
// (which is a fork of Telegram Desktop). Features in this folder are
// ported from Nagram X (https://github.com/risin42/NagramX).
//
// Licensed under GNU GPL v3. See LICENSE for details.
#pragma once

#include <rpl/variable.h>
#include <rpl/producer.h>

#include <QtCore/QString>
#include <QtCore/QJsonObject>

namespace ZKgram {

enum class NameOrder {
	FirstLast = 0,
	LastFirst = 1,
};

// X-macro with every persistent ZKgram option:
// X(type, getter, Setter, defaultValue)
#define ZK_SETTINGS_LIST(X) \
	X(bool, askBeforeCall, AskBeforeCall, false) \
	X(bool, askBeforeBotCommand, AskBeforeBotCommand, false) \
	X(bool, confirmAllLinks, ConfirmAllLinks, false) \
	X(bool, disableNumberRounding, DisableNumberRounding, false) \
	X(int, nameOrder, NameOrder, 0) \
	X(bool, deleteChatForBothSides, DeleteChatForBothSides, false) \
	X(QString, customTitle, CustomTitle, QString()) \
	X(QString, deeplApiKey, DeeplApiKey, QString()) \
	X(QString, llmApiUrl, LlmApiUrl, QStringLiteral("https://api.openai.com/v1")) \
	X(QString, llmApiKey, LlmApiKey, QString()) \
	X(QString, llmModel, LlmModel, QStringLiteral("gpt-4o-mini")) \
	X(QString, llmSystemPrompt, LlmSystemPrompt, QString()) \
	X(int, llmTemperature, LlmTemperature, 30) \
	X(bool, transcribeEnabled, TranscribeEnabled, false) \
	X(QString, transcribeApiUrl, TranscribeApiUrl, QStringLiteral("https://api.groq.com/openai/v1")) \
	X(QString, transcribeApiKey, TranscribeApiKey, QString()) \
	X(QString, transcribeModel, TranscribeModel, QStringLiteral("whisper-large-v3-turbo")) \
	X(QString, transcribeLanguage, TranscribeLanguage, QString())

class Settings final {
public:
	Settings(const Settings &) = delete;
	Settings &operator=(const Settings &) = delete;

	[[nodiscard]] static Settings &Get();

	static void Load();
	static void Save();
	static void Reset();

	[[nodiscard]] static QString FilePath();
	[[nodiscard]] QJsonObject toJson() const;
	void fromJson(const QJsonObject &json);

#define ZK_DECLARE_ACCESSORS(Type, getter, Setter, def) \
	[[nodiscard]] Type getter() const { return _##getter.current(); } \
	[[nodiscard]] rpl::producer<Type> getter##Value() const { \
		return _##getter.value(); \
	} \
	void set##Setter(Type value);
	ZK_SETTINGS_LIST(ZK_DECLARE_ACCESSORS)
#undef ZK_DECLARE_ACCESSORS

	[[nodiscard]] NameOrder resolvedNameOrder() const {
		return (nameOrder() == int(NameOrder::LastFirst))
			? NameOrder::LastFirst
			: NameOrder::FirstLast;
	}

private:
	Settings();

	void applySideEffects();

#define ZK_DECLARE_MEMBER(Type, getter, Setter, def) \
	rpl::variable<Type> _##getter = def;
	ZK_SETTINGS_LIST(ZK_DECLARE_MEMBER)
#undef ZK_DECLARE_MEMBER

};

// Application title used for the main window when no chat is open.
[[nodiscard]] QString AppTitle();

// Full product name, e.g. for the about box.
[[nodiscard]] QString ProductName();

// Links shown in "About" sections.
[[nodiscard]] QString RepositoryUrl();

} // namespace ZKgram
