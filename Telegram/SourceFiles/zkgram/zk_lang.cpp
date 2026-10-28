// This file is part of ZKgram Desktop, a fork of AyuGram Desktop.
//
// Licensed under GNU GPL v3. See LICENSE for details.
#include "zkgram/zk_lang.h"

#include "lang/lang_instance.h"

namespace ZKgram {
namespace {

struct Translation {
	const char *key;
	const char *value;
};

// Russian translations for zk_* keys (English is in lang.strings).
constexpr Translation kRussian[] = {
	{ "zk_CategoryNagram", "Функции Nagram X" },
	{ "zk_SettingsTitle", "Функции Nagram X" },
	{ "zk_ConfirmationsHeader", "Подтверждения" },
	{ "zk_AskBeforeCall", "Спрашивать перед звонком" },
	{ "zk_AskBeforeBotCommand", "Спрашивать перед отправкой команд ботам" },
	{ "zk_ConfirmAllLinks", "Спрашивать перед открытием ссылок" },
	{ "zk_ConfirmAllLinksAbout", "Запрашивает подтверждение перед открытием любой ссылки, включая ссылки t.me и кнопки ботов. Чтобы открыть без вопроса, кликни с зажатым Ctrl." },
	{ "zk_ChatsHeader", "Чаты" },
	{ "zk_DisableNumberRounding", "Не округлять числа" },
	{ "zk_DisableNumberRoundingAbout", "Показывает точное число просмотров, реакций и участников, например 12 345 вместо 12.3K." },
	{ "zk_NameOrder", "Порядок имени" },
	{ "zk_NameOrderFirstLast", "Имя Фамилия" },
	{ "zk_NameOrderLastFirst", "Фамилия Имя" },
	{ "zk_DeleteChatForBothSides", "Удалять чаты у обоих по умолчанию" },
	{ "zk_StickerSize", "Размер стикеров" },
	{ "zk_StickerSizeDefault", "Обычный" },
	{ "zk_WindowTitle", "Заголовок окна" },
	{ "zk_RestartRequiredAbout", "Порядок имени и размер стикеров применяются после перезапуска приложения." },
	{ "zk_TranslatorHeader", "Переводчик" },
	{ "zk_DeeplApiKey", "Ключ DeepL API" },
	{ "zk_DeeplAbout", "Бесплатный ключ DeepL API: deepl.com/pro-api. Ключи с окончанием :fx работают через бесплатный адрес." },
	{ "zk_LlmHeader", "ИИ-переводчик" },
	{ "zk_ApiUrl", "Адрес API" },
	{ "zk_ApiKey", "Ключ API" },
	{ "zk_Model", "Модель" },
	{ "zk_LlmSystemPrompt", "Системный промпт" },
	{ "zk_LlmTemperature", "Температура" },
	{ "zk_LlmAbout", "Подходит любой OpenAI-совместимый API: OpenAI, Google Gemini, Groq, OpenRouter, DeepSeek, Ollama и другие. Выбери «AI» в качестве переводчика выше." },
	{ "zk_NotSet", "Не задано" },
	{ "zk_TranscribeHeader", "Расшифровка голосовых (ИИ)" },
	{ "zk_TranscribeEnable", "Расшифровывать голосовые через ИИ" },
	{ "zk_TranscribeLanguage", "Язык (необязательно)" },
	{ "zk_TranscribeAbout", "Кнопка расшифровки у голосовых и видеосообщений будет работать через OpenAI-совместимый API распознавания речи (Whisper). Бесплатный ключ выдаёт Groq: console.groq.com" },
	{ "zk_TranscribeNotConfigured", "Укажи ключ API для расшифровки в настройках ZKgram." },
	{ "zk_TranscribeFailed", "Не удалось расшифровать сообщение: {item}" },
	{ "zk_BookmarksHeader", "Закладки" },
	{ "zk_Bookmarks", "Закладки" },
	{ "zk_AllBookmarks", "Все закладки" },
	{ "zk_ClearBookmarks", "Удалить все закладки" },
	{ "zk_ClearBookmarksConfirm", "Удалить все закладки этого аккаунта?" },
	{ "zk_AddBookmark", "В закладки" },
	{ "zk_RemoveBookmark", "Убрать из закладок" },
	{ "zk_BookmarkAdded", "Добавлено в закладки." },
	{ "zk_BookmarkRemoved", "Убрано из закладок." },
	{ "zk_NoBookmarks", "Закладок пока нет. Нажми правой кнопкой на сообщение и выбери «В закладки»." },
	{ "zk_BookmarkMedia", "Медиа" },
	{ "zk_BlocklistHeader", "Заблокированные" },
	{ "zk_UnblockAll", "Разблокировать всех" },
	{ "zk_UnblockAllConfirm", "Разблокировать всех пользователей и ботов в этом аккаунте?" },
	{ "zk_UnblockDeleted", "Разблокировать удалённые аккаунты" },
	{ "zk_UnblockDeletedConfirm", "Разблокировать все удалённые аккаунты?" },
	{ "zk_UnblockInProgress", "Разблокирую, подожди..." },
	{ "zk_UnblockDone", "Разблокировано: {item}" },
	{ "zk_LocalNamesHeader", "Локальные имена" },
	{ "zk_SetLocalName", "Задать локальное имя" },
	{ "zk_ResetLocalName", "Сбросить локальное имя" },
	{ "zk_LocalNamePlaceholder", "Имя, которое видишь только ты" },
	{ "zk_ResetLocalNames", "Сбросить все локальные имена" },
	{ "zk_LocalNamesReset", "Локальные имена сброшены." },
	{ "zk_GoToMessage", "Перейти к сообщению по ID" },
	{ "zk_MessageIdPlaceholder", "ID сообщения" },
	{ "zk_BackupHeader", "Резервная копия" },
	{ "zk_ExportSettings", "Экспорт настроек" },
	{ "zk_ImportSettings", "Импорт настроек" },
	{ "zk_SettingsExported", "Настройки экспортированы." },
	{ "zk_SettingsImported", "Настройки импортированы. Перезапусти приложение, чтобы они применились." },
	{ "zk_SettingsImportFailed", "Не удалось импортировать настройки из этого файла." },
	{ "zk_RestartApp", "Перезапустить приложение" },
	{ "zk_AboutHeader", "О приложении" },
	{ "zk_SourceCode", "Исходный код" },
	{ "zk_BasedOnAyuGram", "AyuGram Desktop" },
	{ "zk_BasedOnNagram", "Nagram X" },
	{ "zk_ConfirmCallText", "Позвонить {user}?" },
	{ "zk_ConfirmBotCommandText", "Отправить команду {item}?" },

	// Rebranded AyuGram strings.
	{ "ayu_AyuPreferences", "Настройки ZKgram" },
	{ "ayu_SettingsWatermark", "ZKgram основан на AyuGram Desktop от Radolyn Labs и на Nagram X." },
	{ "ayu_SettingsDescription", "Форк Telegram Desktop с функциями AyuGram и Nagram X." },
	{ "ayu_FirstLaunchAlert", "ZKgram — **неофициальный** клиент Telegram для личного использования. Ты берёшь на себя **всю ответственность** за использование этого приложения со своим аккаунтом." },
	{ "ayu_IntroAbout", "Добро пожаловать в ZKgram Desktop.\nСделай Telegram своим." },
	{ "ayu_AboutText1", "Неофициальный клиент Telegram на основе {api_link}." },
};

// AyuGram strings whose translations mention the AyuGram brand
// in places where ZKgram should be shown instead.
constexpr const char *kRebrandKeys[] = {
	"ayu_AyuPreferences",
	"ayu_ResetSettingsConfirmation",
	"ayu_ExportDataTitle",
	"ayu_ImportDataTitle",
	"ayu_AyuAttachments",
	"ayu_AyuDatabase",
	"ayu_FirstLaunchAlert",
	"ayu_IntroAbout",
	"ayu_UpdateAyuGram",
	"ayu_SettingsDescription",
};

[[nodiscard]] bool IsRussian(const QString &id) {
	return id.startsWith(u"ru"_q, Qt::CaseInsensitive);
}

} // namespace

void ApplyTranslations() {
	auto &instance = ::Lang::GetInstance();

	for (const auto key : kRebrandKeys) {
		const auto keyBytes = QByteArray(key);
		const auto current = instance.getNonDefaultValue(keyBytes);
		if (!current.isEmpty() && current.contains(u"AyuGram"_q)) {
			auto replaced = current;
			replaced.replace(u"AyuGram"_q, u"ZKgram"_q);
			instance.applyValue(keyBytes, replaced.toUtf8());
		}
	}

	const auto id = instance.id();
	const auto baseId = instance.baseId();
	if (IsRussian(id) || (id.isEmpty() && IsRussian(baseId))) {
		for (const auto &entry : kRussian) {
			instance.applyValue(
				QByteArray(entry.key),
				QByteArray(entry.value));
		}
	}
}

} // namespace ZKgram
