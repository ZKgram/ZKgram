// This file is part of ZKgram Desktop, a fork of AyuGram Desktop
// (which is a fork of Telegram Desktop). Features in this folder are
// ported from Nagram X (https://github.com/risin42/NagramX).
//
// Licensed under GNU GPL v3. See LICENSE for details.
#include "zkgram/zk_settings.h"

#include "lang/lang_tag.h"
#include "settings.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonValue>
#include <QtCore/QSaveFile>

namespace ZKgram {
namespace {

void ReadValue(const QJsonObject &json, const char *key, bool &out) {
	const auto value = json.value(QLatin1String(key));
	if (value.isBool()) {
		out = value.toBool();
	}
}

void ReadValue(const QJsonObject &json, const char *key, int &out) {
	const auto value = json.value(QLatin1String(key));
	if (value.isDouble()) {
		out = value.toInt();
	}
}

void ReadValue(const QJsonObject &json, const char *key, QString &out) {
	const auto value = json.value(QLatin1String(key));
	if (value.isString()) {
		out = value.toString();
	}
}

} // namespace

Settings::Settings() = default;

Settings &Settings::Get() {
	static Settings instance;
	return instance;
}

QString Settings::FilePath() {
	return cWorkingDir() + u"tdata/zkgram_settings.json"_q;
}

QJsonObject Settings::toJson() const {
	auto result = QJsonObject();
#define ZK_WRITE(Type, getter, Setter, def) \
	result.insert(QStringLiteral(#getter), QJsonValue(_##getter.current()));
	ZK_SETTINGS_LIST(ZK_WRITE)
#undef ZK_WRITE
	return result;
}

void Settings::fromJson(const QJsonObject &json) {
#define ZK_READ(Type, getter, Setter, def) \
	{ \
		auto value = _##getter.current(); \
		ReadValue(json, #getter, value); \
		_##getter = value; \
	}
	ZK_SETTINGS_LIST(ZK_READ)
#undef ZK_READ
	applySideEffects();
}

void Settings::Load() {
	auto &settings = Get();
	auto file = QFile(FilePath());
	if (file.open(QIODevice::ReadOnly)) {
		auto error = QJsonParseError();
		const auto document = QJsonDocument::fromJson(file.readAll(), &error);
		file.close();
		if (error.error == QJsonParseError::NoError && document.isObject()) {
			settings.fromJson(document.object());
			return;
		}
		LOG(("ZKgram: could not parse %1").arg(FilePath()));
	}
	settings.applySideEffects();
}

void Settings::Save() {
	const auto &settings = Get();
	auto file = QSaveFile(FilePath());
	if (!file.open(QIODevice::WriteOnly)) {
		LOG(("ZKgram: could not write %1").arg(FilePath()));
		return;
	}
	file.write(QJsonDocument(settings.toJson()).toJson(
		QJsonDocument::Indented));
	file.commit();
}

void Settings::Reset() {
	auto &settings = Get();
#define ZK_RESET(Type, getter, Setter, def) \
	settings._##getter = Type(def);
	ZK_SETTINGS_LIST(ZK_RESET)
#undef ZK_RESET
	settings.applySideEffects();
	Save();
}

void Settings::applySideEffects() {
	Lang::SetCountShorteningDisabled(_disableNumberRounding.current());
}

#define ZK_DEFINE_SETTER(Type, getter, Setter, def) \
void Settings::set##Setter(Type value) { \
	_##getter = std::move(value); \
	applySideEffects(); \
	Save(); \
}
ZK_SETTINGS_LIST(ZK_DEFINE_SETTER)
#undef ZK_DEFINE_SETTER

QString AppTitle() {
	const auto custom = Settings::Get().customTitle().trimmed();
	return custom.isEmpty() ? u"ZKgram"_q : custom;
}

QString ProductName() {
	return u"ZKgram Desktop"_q;
}

QString RepositoryUrl() {
	return u"https://github.com/ZKgram/ZKgram"_q;
}

} // namespace ZKgram
