#include "util/Settings.h"

#include "ModelControllerSettings.h"

namespace HomeCompa::Flibrary {

ModelControllerSettings::ModelControllerSettings(Settings & uiSettings
#define	MODEL_CONTROLLER_SETTINGS_ITEM(NAME) , const char * NAME, const QVariant & NAME##Default
		MODEL_CONTROLLER_SETTINGS_ITEMS_XMACRO
#undef  MODEL_CONTROLLER_SETTINGS_ITEM
)
	: m_uiSettings(uiSettings)
#define	MODEL_CONTROLLER_SETTINGS_ITEM(NAME) , m_##NAME(NAME), m_##NAME##Default(NAME##Default)
		MODEL_CONTROLLER_SETTINGS_ITEMS_XMACRO
#undef  MODEL_CONTROLLER_SETTINGS_ITEM
{
	m_uiSettings.RegisterObserver(this);
}

ModelControllerSettings::~ModelControllerSettings()
{
	m_uiSettings.UnregisterObserver(this);
}

void ModelControllerSettings::HandleValueChanged(const QString & key, const QVariant & value)
{
#define	MODEL_CONTROLLER_SETTINGS_ITEM(NAME) if (key == m_##NAME) return Perform(&IModelControllerSettingsObserver::handle_##NAME##Changed, std::cref(value));
		MODEL_CONTROLLER_SETTINGS_ITEMS_XMACRO
#undef  MODEL_CONTROLLER_SETTINGS_ITEM
}

#define MODEL_CONTROLLER_SETTINGS_ITEM(NAME) \
QVariant ModelControllerSettings::NAME() const { return m_uiSettings.Get(m_##NAME, m_##NAME##Default); } \
void ModelControllerSettings::set_##NAME(const QVariant & value) { m_uiSettings.Set(m_##NAME, value); }
		MODEL_CONTROLLER_SETTINGS_ITEMS_XMACRO
#undef  MODEL_CONTROLLER_SETTINGS_ITEM

}
