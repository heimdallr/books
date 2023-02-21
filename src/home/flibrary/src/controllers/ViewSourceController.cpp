#include <QQmlEngine>

#include <plog/Log.h>

#include "util/Settings.h"
#include "util/ISettingsObserver.h"

#include "ComboBoxController.h"
#include "ViewSourceController.h"

namespace HomeCompa::Flibrary {

class ViewSourceController::Impl final
	: ISettingsObserver
	, IComboBoxDataProvider
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

		PLOGD << "ViewSourceController created: " << m_keyName;
	}

	~Impl() override
	{
		comboBoxController.UnregisterObserver(this);
		m_uiSetting.UnregisterObserver(this);

		PLOGD << "ViewSourceController destroyed: " << m_keyName;
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

	const QString & GetTitleDefault(const QString & /*value*/) const override
	{
		static const QString defaultTitle;
		return defaultTitle;
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
