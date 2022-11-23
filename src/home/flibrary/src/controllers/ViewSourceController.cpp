#include <QQmlEngine>

#include "util/Settings.h"
#include "util/SettingsObserver.h"

#include "ComboBoxController.h"
#include "ViewSourceController.h"

namespace HomeCompa::Flibrary {

class ViewSourceController::Impl final
	: SettingsObserver
	, ComboBoxDataProvider
	, ComboBoxObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	ComboBoxController comboBoxController;

public:
	Impl(Settings & uiSetting, const char * keyName, const QVariant & defaultValue, SimpleModelItems simpleModelItems)
		: comboBoxController(*this)
		, m_uiSetting(uiSetting)
		, m_keyName(keyName)
		, m_defaultValue(defaultValue)
	{
		QQmlEngine::setObjectOwnership(&comboBoxController, QQmlEngine::CppOwnership);

		comboBoxController.RegisterObserver(this);
		comboBoxController.SetData(std::move(simpleModelItems));
		m_uiSetting.RegisterObserver(this);
	}

	~Impl() override
	{
		comboBoxController.UnregisterObserver(this);
		m_uiSetting.UnregisterObserver(this);
	}

private: // SettingsObserver
	void HandleValueChanged(const QString & key, const QVariant & /*value*/) override
	{
		if (key == m_keyName)
			comboBoxController.OnValueChanged();
	}

private: // ComboBoxDataProvider
	QString GetValue() const override
	{
		return m_uiSetting.Get(m_keyName, m_defaultValue).toString();
	}

private: // ComboBoxObserver
	void SetValue(const QString & value) override
	{
		m_uiSetting.Set(m_keyName, value);
	}

private:
	Settings & m_uiSetting;
	const char * const m_keyName;
	const QVariant & m_defaultValue;
};

ViewSourceController::ViewSourceController(Settings & uiSetting, const char * keyName, const QVariant & defaultValue, SimpleModelItems simpleModelItems)
	: m_impl(uiSetting, keyName, defaultValue, std::move(simpleModelItems))
{
}

ViewSourceController::~ViewSourceController() = default;

ComboBoxController * ViewSourceController::GetComboBoxController() noexcept
{
	return &m_impl->comboBoxController;
}

}
