// This file is part of ZKgram Desktop, a fork of AyuGram Desktop.
// Settings page with features ported from Nagram X.
//
// Licensed under GNU GPL v3. See LICENSE for details.
#include "zkgram/zk_settings_section.h"

#include "ayu/ayu_settings.h"
#include "ayu/ui/boxes/edit_mark_box.h"
#include "ayu/ui/settings/ayu_builder.h"
#include "ayu/ui/settings/settings_main.h"
#include "base/options.h"
#include "core/application.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "mainwindow.h"
#include "settings/settings_builder.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/boxes/confirm_box.h"
#include "ui/ui_utility.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "zkgram/zk_bookmarks.h"
#include "zkgram/zk_features.h"
#include "zkgram/zk_settings.h"

#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>

namespace Settings {
namespace {

using namespace Builder;
using namespace AyuBuilder;

constexpr int kStickerSizes[] = { 0, 96, 128, 160, 192, 224, 256 };
constexpr auto kStickerSizeOption = "sticker-size";

[[nodiscard]] ::ZKgram::Settings &Zk() {
	return ::ZKgram::Settings::Get();
}

[[nodiscard]] rpl::producer<QString> TextLabel(
		rpl::producer<QString> value,
		bool secret) {
	return std::move(value) | rpl::map([=](const QString &text) {
		const auto trimmed = text.trimmed();
		if (trimmed.isEmpty()) {
			return tr::zk_NotSet(tr::now);
		} else if (secret) {
			return QString::fromUtf8("\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2")
				+ trimmed.right(4);
		} else if (trimmed.size() > 32) {
			return trimmed.left(30) + QString::fromUtf8("\xE2\x80\xA6");
		}
		return trimmed;
	});
}

void AddTextSetting(
		SectionBuilder &builder,
		QString id,
		rpl::producer<QString> title,
		rpl::producer<QString> value,
		Fn<QString()> current,
		QString defaultValue,
		Fn<void(QString)> save,
		bool secret) {
	const auto controller = builder.controller();
	auto boxTitle = rpl::duplicate(title);
	builder.addButton({
		.id = std::move(id),
		.title = std::move(title),
		.st = &st::settingsButtonNoIcon,
		.label = TextLabel(std::move(value), secret),
		.onClick = [=, boxTitle = std::move(boxTitle)]() mutable {
			if (!controller) {
				return;
			}
			controller->show(Box<EditMarkBox>(
				rpl::duplicate(boxTitle),
				current(),
				defaultValue,
				[=](const QString &value) { save(value.trimmed()); }));
		},
	});
}

void AddZkToggle(
		AyuSectionBuilder &ayu,
		QString id,
		rpl::producer<QString> title,
		Fn<bool()> getter,
		Fn<void(bool)> setter) {
	ayu.addToggle({
		.id = std::move(id),
		.title = std::move(title),
		.getter = std::move(getter),
		.setter = std::move(setter),
	});
}

void BuildConfirmations(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::zk_ConfirmationsHeader());

	AddZkToggle(
		ayu,
		u"zk/askBeforeCall"_q,
		tr::zk_AskBeforeCall(),
		[] { return Zk().askBeforeCall(); },
		[](bool v) { Zk().setAskBeforeCall(v); });
	AddZkToggle(
		ayu,
		u"zk/askBeforeBotCommand"_q,
		tr::zk_AskBeforeBotCommand(),
		[] { return Zk().askBeforeBotCommand(); },
		[](bool v) { Zk().setAskBeforeBotCommand(v); });
	AddZkToggle(
		ayu,
		u"zk/confirmAllLinks"_q,
		tr::zk_ConfirmAllLinks(),
		[] { return Zk().confirmAllLinks(); },
		[](bool v) { Zk().setConfirmAllLinks(v); });

	builder.addSkip();
	builder.addDividerText(tr::zk_ConfirmAllLinksAbout());
	builder.addSkip();
}

void BuildChats(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::zk_ChatsHeader());

	AddZkToggle(
		ayu,
		u"zk/disableNumberRounding"_q,
		tr::zk_DisableNumberRounding(),
		[] { return Zk().disableNumberRounding(); },
		[](bool v) { Zk().setDisableNumberRounding(v); });
	AddZkToggle(
		ayu,
		u"zk/deleteChatForBothSides"_q,
		tr::zk_DeleteChatForBothSides(),
		[] { return Zk().deleteChatForBothSides(); },
		[](bool v) { Zk().setDeleteChatForBothSides(v); });

	ayu.addChooseButton({
		.id = u"zk/nameOrder"_q,
		.title = tr::zk_NameOrder(),
		.boxTitle = tr::zk_NameOrder(),
		.initialSelection = (Zk().nameOrder() == 1) ? 1 : 0,
		.options = {
			tr::zk_NameOrderFirstLast(tr::now),
			tr::zk_NameOrderLastFirst(tr::now),
		},
		.setter = [](int index) { Zk().setNameOrder(index); },
	});

	AddTextSetting(
		builder,
		u"zk/windowTitle"_q,
		tr::zk_WindowTitle(),
		Zk().customTitleValue() | rpl::map([](const QString &value) {
			return value.trimmed().isEmpty() ? u"ZKgram"_q : value;
		}),
		[] { return ::ZKgram::AppTitle(); },
		u"ZKgram"_q,
		[](QString value) {
			Zk().setCustomTitle((value == u"ZKgram"_q) ? QString() : value);
			if (const auto window = Core::App().activePrimaryWindow()) {
				window->widget()->updateTitle();
			}
		},
		false);

	auto stickerSize = base::options::lookup<int>(kStickerSizeOption).value();
	ayu.addSlider({
		.id = u"zk/stickerSize"_q,
		.title = tr::zk_StickerSize(),
		.steps = int(std::size(kStickerSizes)),
		.current = stickerSize,
		.indexToValue = [](int index) {
			return kStickerSizes[std::clamp(
				index,
				0,
				int(std::size(kStickerSizes)) - 1)];
		},
		.onFinalChanged = [](int value) {
			base::options::lookup<int>(kStickerSizeOption).set(value);
		},
		.formatLabel = [](int value) {
			return value
				? QString::number(value)
				: tr::zk_StickerSizeDefault(tr::now);
		},
	});

	builder.addSkip();
	builder.addDividerText(tr::zk_RestartRequiredAbout());
	builder.addSkip();
}

void BuildTranslator(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::zk_TranslatorHeader());

	const auto providers = std::vector<std::pair<TranslationProvider, QString>>{
		{ TranslationProvider::Telegram, u"Telegram"_q },
		{ TranslationProvider::Google, u"Google"_q },
		{ TranslationProvider::Yandex, u"Yandex"_q },
		{ TranslationProvider::DeepL, u"DeepL"_q },
		{ TranslationProvider::Microsoft, u"Microsoft"_q },
		{ TranslationProvider::Llm, u"AI"_q },
	};
	auto labels = std::vector<QString>();
	auto initial = 0;
	const auto current = AyuSettings::getInstance().translationProvider();
	for (auto i = 0; i != int(providers.size()); ++i) {
		labels.push_back(providers[i].second);
		if (providers[i].first == current) {
			initial = i;
		}
	}
	ayu.addChooseButton({
		.id = u"zk/translationProvider"_q,
		.title = tr::ayu_TranslationProvider(),
		.boxTitle = tr::ayu_TranslationProvider(),
		.initialSelection = initial,
		.options = labels,
		.setter = [=](int index) {
			if (index >= 0 && index < int(providers.size())) {
				AyuSettings::getInstance().setTranslationProvider(
					providers[index].first);
			}
		},
	});

	AddTextSetting(
		builder,
		u"zk/deeplApiKey"_q,
		tr::zk_DeeplApiKey(),
		Zk().deeplApiKeyValue(),
		[] { return Zk().deeplApiKey(); },
		QString(),
		[](QString value) { Zk().setDeeplApiKey(value); },
		true);

	builder.addSkip();
	builder.addDividerText(tr::zk_DeeplAbout());
	builder.addSkip();

	builder.addSubsectionTitle(tr::zk_LlmHeader());
	AddTextSetting(
		builder,
		u"zk/llmApiUrl"_q,
		tr::zk_ApiUrl(),
		Zk().llmApiUrlValue(),
		[] { return Zk().llmApiUrl(); },
		u"https://api.openai.com/v1"_q,
		[](QString value) { Zk().setLlmApiUrl(value); },
		false);
	AddTextSetting(
		builder,
		u"zk/llmApiKey"_q,
		tr::zk_ApiKey(),
		Zk().llmApiKeyValue(),
		[] { return Zk().llmApiKey(); },
		QString(),
		[](QString value) { Zk().setLlmApiKey(value); },
		true);
	AddTextSetting(
		builder,
		u"zk/llmModel"_q,
		tr::zk_Model(),
		Zk().llmModelValue(),
		[] { return Zk().llmModel(); },
		u"gpt-4o-mini"_q,
		[](QString value) { Zk().setLlmModel(value); },
		false);
	AddTextSetting(
		builder,
		u"zk/llmSystemPrompt"_q,
		tr::zk_LlmSystemPrompt(),
		Zk().llmSystemPromptValue(),
		[] { return Zk().llmSystemPrompt(); },
		QString(),
		[](QString value) { Zk().setLlmSystemPrompt(value); },
		false);

	ayu.addSlider({
		.id = u"zk/llmTemperature"_q,
		.title = tr::zk_LlmTemperature(),
		.steps = 21,
		.current = Zk().llmTemperature(),
		.indexToValue = [](int index) { return index * 10; },
		.onFinalChanged = [](int value) { Zk().setLlmTemperature(value); },
		.formatLabel = [](int value) {
			return QString::number(value / 100., 'f', 1);
		},
	});

	builder.addSkip();
	builder.addDividerText(tr::zk_LlmAbout());
	builder.addSkip();
}

void BuildTranscription(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::zk_TranscribeHeader());

	AddZkToggle(
		ayu,
		u"zk/transcribeEnabled"_q,
		tr::zk_TranscribeEnable(),
		[] { return Zk().transcribeEnabled(); },
		[](bool v) { Zk().setTranscribeEnabled(v); });
	AddTextSetting(
		builder,
		u"zk/transcribeApiUrl"_q,
		tr::zk_ApiUrl(),
		Zk().transcribeApiUrlValue(),
		[] { return Zk().transcribeApiUrl(); },
		u"https://api.groq.com/openai/v1"_q,
		[](QString value) { Zk().setTranscribeApiUrl(value); },
		false);
	AddTextSetting(
		builder,
		u"zk/transcribeApiKey"_q,
		tr::zk_ApiKey(),
		Zk().transcribeApiKeyValue(),
		[] { return Zk().transcribeApiKey(); },
		QString(),
		[](QString value) { Zk().setTranscribeApiKey(value); },
		true);
	AddTextSetting(
		builder,
		u"zk/transcribeModel"_q,
		tr::zk_Model(),
		Zk().transcribeModelValue(),
		[] { return Zk().transcribeModel(); },
		u"whisper-large-v3-turbo"_q,
		[](QString value) { Zk().setTranscribeModel(value); },
		false);
	AddTextSetting(
		builder,
		u"zk/transcribeLanguage"_q,
		tr::zk_TranscribeLanguage(),
		Zk().transcribeLanguageValue(),
		[] { return Zk().transcribeLanguage(); },
		QString(),
		[](QString value) { Zk().setTranscribeLanguage(value); },
		false);

	builder.addSkip();
	builder.addDividerText(tr::zk_TranscribeAbout());
	builder.addSkip();
}

void BuildTools(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	const auto controller = builder.controller();

	builder.addSubsectionTitle(tr::zk_BookmarksHeader());
	builder.addButton({
		.id = u"zk/allBookmarks"_q,
		.title = tr::zk_AllBookmarks(),
		.icon = { &st::menuIconFave },
		.onClick = [=] {
			if (controller) {
				::ZKgram::ShowBookmarksBox(controller, nullptr);
			}
		},
	});
	builder.addButton({
		.id = u"zk/clearBookmarks"_q,
		.title = tr::zk_ClearBookmarks(),
		.icon = { &st::menuIconDelete },
		.onClick = [=] {
			if (!controller) {
				return;
			}
			const auto session = &controller->session();
			controller->show(Ui::MakeConfirmBox({
				.text = tr::zk_ClearBookmarksConfirm(tr::now),
				.confirmed = [=](Fn<void()> close) {
					::ZKgram::ClearBookmarks(session);
					close();
				},
				.confirmText = tr::lng_box_delete(),
			}));
		},
	});

	ayu.addSectionDivider();

	builder.addSubsectionTitle(tr::zk_BlocklistHeader());
	builder.addButton({
		.id = u"zk/unblockAll"_q,
		.title = tr::zk_UnblockAll(),
		.icon = { &st::menuIconUnblock },
		.onClick = [=] {
			if (controller) {
				::ZKgram::UnblockAll(controller, false);
			}
		},
	});
	builder.addButton({
		.id = u"zk/unblockDeleted"_q,
		.title = tr::zk_UnblockDeleted(),
		.icon = { &st::menuIconRemove },
		.onClick = [=] {
			if (controller) {
				::ZKgram::UnblockAll(controller, true);
			}
		},
	});

	ayu.addSectionDivider();

	builder.addSubsectionTitle(tr::zk_LocalNamesHeader());
	builder.addButton({
		.id = u"zk/resetLocalNames"_q,
		.title = tr::zk_ResetLocalNames(),
		.icon = { &st::menuIconRestore },
		.onClick = [=] {
			if (!controller) {
				return;
			}
			::ZKgram::ResetAllLocalNames(&controller->session());
			controller->showToast(tr::zk_LocalNamesReset(tr::now));
		},
	});

	ayu.addSectionDivider();
}

void BuildBackup(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	const auto controller = builder.controller();

	builder.addSubsectionTitle(tr::zk_BackupHeader());
	builder.addButton({
		.id = u"zk/exportSettings"_q,
		.title = tr::zk_ExportSettings(),
		.icon = { &st::menuIconExport },
		.onClick = [=] {
			if (controller) {
				::ZKgram::ExportSettings(controller);
			}
		},
	});
	builder.addButton({
		.id = u"zk/importSettings"_q,
		.title = tr::zk_ImportSettings(),
		.icon = { &st::menuIconImportTheme },
		.onClick = [=] {
			if (controller) {
				::ZKgram::ImportSettings(controller);
			}
		},
	});
	builder.addButton({
		.id = u"zk/restartApp"_q,
		.title = tr::zk_RestartApp(),
		.icon = { &st::menuIconRestartBot },
		.onClick = [] { Core::Restart(); },
	});

	ayu.addSectionDivider();
}

void BuildAbout(SectionBuilder &builder) {
	builder.addSubsectionTitle(tr::zk_AboutHeader());
	builder.addButton({
		.id = u"zk/sourceCode"_q,
		.title = tr::zk_SourceCode(),
		.icon = { &st::menuIconLinks },
		.label = rpl::single(u"GitHub"_q),
		.onClick = [] {
			QDesktopServices::openUrl(QUrl(::ZKgram::RepositoryUrl()));
		},
	});
	builder.addButton({
		.id = u"zk/basedOnAyuGram"_q,
		.title = tr::zk_BasedOnAyuGram(),
		.icon = { &st::menuIconGroupReactions },
		.label = rpl::single(u"GPL-3.0"_q),
		.onClick = [] {
			QDesktopServices::openUrl(
				QUrl(u"https://github.com/AyuGram/AyuGramDesktop"_q));
		},
	});
	builder.addButton({
		.id = u"zk/basedOnNagram"_q,
		.title = tr::zk_BasedOnNagram(),
		.icon = { &st::menuIconCustomize },
		.label = rpl::single(u"GPL-3.0"_q),
		.onClick = [] {
			QDesktopServices::openUrl(
				QUrl(u"https://github.com/risin42/NagramX"_q));
		},
	});
	builder.addSkip();
}

const auto kMeta = BuildHelper({
	.id = ZkNagram::Id(),
	.parentId = AyuMain::Id(),
	.title = &tr::zk_SettingsTitle,
	.icon = &st::menuIconCustomize,
}, [](SectionBuilder &builder) {
	auto ayu = AyuSectionBuilder(builder);

	builder.addSkip();
	BuildConfirmations(builder, ayu);
	BuildChats(builder, ayu);
	BuildTranslator(builder, ayu);
	BuildTranscription(builder, ayu);
	BuildTools(builder, ayu);
	BuildBackup(builder, ayu);
	BuildAbout(builder);
});

} // namespace

rpl::producer<QString> ZkNagram::title() {
	return tr::zk_SettingsTitle();
}

ZkNagram::ZkNagram(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void ZkNagram::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type ZkNagramId() {
	return ZkNagram::Id();
}

} // namespace Settings
