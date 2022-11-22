#include <QAbstractItemModel>
#include <QQmlEngine>

#include "util/Settings.h"
#include "util/SettingsObserver.h"

#include "ViewSourceController.h"

namespace HomeCompa::Flibrary {

struct ViewSourceController::Impl final
	: private SettingsObserver
{
	NON_COPY_MOVABLE(Impl)

private:
	ViewSourceController & m_self;

public:
	Settings & uiSetting;
	const char * const keyName;
	const QVariant & defaultValue;

	SimpleModelItems simpleModelItems;
	QAbstractItemModel * model { CreateSimpleModel(simpleModelItems, &m_self) };

	Impl(ViewSourceController & self, Settings & uiSetting_, const char * keyName_, const QVariant & defaultValue_, SimpleModelItems simpleModelItems_)
		: m_self(self)
		, uiSetting(uiSetting_)
		, keyName(keyName_)
		, defaultValue(defaultValue_)
		, simpleModelItems(std::move(simpleModelItems_))
	{
		QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
		uiSetting.RegisterObserver(this);
	}

	~Impl() override
	{
		uiSetting.UnregisterObserver(this);
	}

private:
	void HandleValueChanged(const QString & key, const QVariant & /*value*/) override
	{
		if (key != keyName)
			return;

		emit m_self.ValueChanged();
		emit m_self.TitleChanged();
	}
};

ViewSourceController::ViewSourceController(Settings & uiSetting, const char * keyName, const QVariant & defaultValue, SimpleModelItems simpleModelItems, QObject * parent)
	: QObject(parent)
	, m_impl(*this, uiSetting, keyName, defaultValue, std::move(simpleModelItems))
{
}

ViewSourceController::~ViewSourceController() = default;

QAbstractItemModel * ViewSourceController::GetModel() const
{
	return m_impl->model;
}

const QString & ViewSourceController::GetTitle() const
{
	const auto it = std::ranges::find_if(std::as_const(m_impl->simpleModelItems), [value = GetValue()](const SimpleModelItem & item)
	{
		return item.Value == value;
	});
	assert(it != std::cend(m_impl->simpleModelItems));
	return it->Title;
}

QString ViewSourceController::GetValue() const
{
	return m_impl->uiSetting.Get(m_impl->keyName, m_impl->defaultValue).toString();
}

void ViewSourceController::SetValue(const QString & value)
{
	m_impl->uiSetting.Set(m_impl->keyName, value);
}

}
