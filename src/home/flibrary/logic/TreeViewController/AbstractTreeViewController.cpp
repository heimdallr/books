#include "AbstractTreeViewController.h"

#include <QModelIndex>

#include "interface/constants/SettingsConstant.h"

using namespace HomeCompa::Flibrary;

struct AbstractTreeViewController::Impl final
{
	AbstractTreeViewController& self;
	IObserver*                  observer { nullptr };
	QString                     settingsModeKey { QString(Constant::Settings::VIEW_MODE_KEY_TEMPLATE).arg(self.m_context) };

	explicit Impl(AbstractTreeViewController& self)
		: self(self)
	{
	}
};

AbstractTreeViewController::AbstractTreeViewController(const char* const context, std::shared_ptr<ISettings> settings, const std::shared_ptr<IModelProvider>& modelProvider)
	: m_context { context }
	, m_settings { std::move(settings) }
	, m_modelProvider { modelProvider }
	, m_impl { *this }
{
}

AbstractTreeViewController::~AbstractTreeViewController() = default;

const char* AbstractTreeViewController::TrContext() const noexcept
{
	return m_context;
}

int AbstractTreeViewController::GetModeIndex() const
{
	return GetModeIndex(m_settings->Get(m_impl->settingsModeKey).toString());
}

void AbstractTreeViewController::RegisterObserver(IObserver* observer)
{
	m_impl->observer = observer;
	Register(observer);
}

void AbstractTreeViewController::UnregisterObserver(IObserver* observer)
{
	m_impl->observer = nullptr;
	Unregister(observer);
}

void AbstractTreeViewController::Setup()
{
	OnModeChanged(m_settings->Get(m_impl->settingsModeKey).toString());
}

void AbstractTreeViewController::SetMode(const QString& mode)
{
	m_settings->Set(m_impl->settingsModeKey, mode);
	OnModeChanged(mode);
}

QAbstractItemModel* AbstractTreeViewController::GetModel() const noexcept
{
	return m_impl->observer ? m_impl->observer->GetModel() : nullptr;
}

QModelIndex AbstractTreeViewController::GetCurrentIndex() const noexcept
{
	return m_impl->observer ? m_impl->observer->GetCurrentIndex() : QModelIndex {};
}
