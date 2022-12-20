#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/observable.h"
#include "fnd/observer.h"

#include "util/SettingsObserver.h"

class QVariant;

namespace HomeCompa {
class Settings;
}

namespace HomeCompa::Flibrary {

#define MODEL_CONTROLLER_SETTINGS_ITEMS_XMACRO        \
		MODEL_CONTROLLER_SETTINGS_ITEM(viewMode)      \
		MODEL_CONTROLLER_SETTINGS_ITEM(viewModeValue) \
		MODEL_CONTROLLER_SETTINGS_ITEM(viewSource)    \
		MODEL_CONTROLLER_SETTINGS_ITEM(id)            \


class ModelControllerSettingsObserver
	: public Observer
{
public:
	virtual ~ModelControllerSettingsObserver() = default;
#define MODEL_CONTROLLER_SETTINGS_ITEM(NAME) virtual void handle_##NAME##Changed(const QVariant & value) = 0;
		MODEL_CONTROLLER_SETTINGS_ITEMS_XMACRO
#undef  MODEL_CONTROLLER_SETTINGS_ITEM
};

class ModelControllerSettings
	: public Observable<ModelControllerSettingsObserver>
	, SettingsObserver
{
	NON_COPY_MOVABLE(ModelControllerSettings)

public:
	ModelControllerSettings(Settings & uiSettings
#define	MODEL_CONTROLLER_SETTINGS_ITEM(NAME) , const char * NAME, const QVariant & NAME##Default
		MODEL_CONTROLLER_SETTINGS_ITEMS_XMACRO
#undef  MODEL_CONTROLLER_SETTINGS_ITEM
	);

	~ModelControllerSettings() override;

#define MODEL_CONTROLLER_SETTINGS_ITEM(NAME) QVariant NAME() const; void set_##NAME(const QVariant & value);
		MODEL_CONTROLLER_SETTINGS_ITEMS_XMACRO
#undef  MODEL_CONTROLLER_SETTINGS_ITEM

private: // SettingsObserver
	void HandleValueChanged(const QString & key, const QVariant & value) override;

private:
	Settings & m_uiSettings;
#define MODEL_CONTROLLER_SETTINGS_ITEM(NAME) const char * m_##NAME; const QVariant & m_##NAME##Default;
		MODEL_CONTROLLER_SETTINGS_ITEMS_XMACRO
#undef  MODEL_CONTROLLER_SETTINGS_ITEM
};

}
