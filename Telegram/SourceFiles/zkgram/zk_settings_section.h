// This file is part of ZKgram Desktop, a fork of AyuGram Desktop.
// Settings page with features ported from Nagram X.
//
// Licensed under GNU GPL v3. See LICENSE for details.
#pragma once

#include "settings/settings_common.h"
#include "settings/settings_common_session.h"

namespace Window {
class SessionController;
} // namespace Window

namespace Settings {

class ZkNagram : public Section<ZkNagram> {
public:
	ZkNagram(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();

};

[[nodiscard]] Type ZkNagramId();

} // namespace Settings
