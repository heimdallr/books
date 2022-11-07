#pragma once

namespace HomeCompa {
class Settings;
}

namespace HomeCompa::Flibrary {

class SettingsProvider
{
public:
	virtual ~SettingsProvider() = default;
	virtual Settings & GetSettings() = 0;
};

}
