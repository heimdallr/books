#pragma once

namespace HomeCompa {
class Settings;
}

namespace HomeCompa::Flibrary {

class ISettingsProvider
{
public:
	virtual ~ISettingsProvider() = default;
	virtual Settings & GetSettings() = 0;
	virtual Settings & GetUiSettings() = 0;
};

}
