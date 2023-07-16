#include "AbstractTreeViewController.h"

#include "fnd/FindPair.h"
#include "fnd/observable.h"

#include "interface/constants/SettingsConstant.h"

#include "data/DataProvider.h"

#include "util/ISettings.h"
#include "util/ISettingsObserver.h"

using namespace HomeCompa::Flibrary;

struct AbstractTreeViewController::Impl final
	: ISettingsObserver
	, Observable<ITreeViewController::IObserver>
{
	AbstractTreeViewController & self;
	QString settingsModeKey { QString(Constant::Settings::VIEW_MODE_KEY_TEMPLATE).arg(self.m_context) };

	explicit Impl(AbstractTreeViewController & self)
		: self(self)
	{
	}

private: // ISettingsObserver
	void HandleValueChanged(const QString & key, const QVariant & value) override
	{
		const auto handler = FindSecond(m_observerMapping, key, &AbstractTreeViewController::Stub);
		(self.*handler)(value);
	}

private:
	using ObserverHandler = void(AbstractTreeViewController::*)(const QVariant &);
	using ObserverHandlerMapping = std::vector<std::pair<QString, ObserverHandler>>;
	[[nodiscard]] ObserverHandlerMapping CreateObserverHandlerMapping() const
	{
		return ObserverHandlerMapping
		{
			{settingsModeKey, &AbstractTreeViewController::OnModeChanged}
		};
	}

private:
	ObserverHandlerMapping m_observerMapping { CreateObserverHandlerMapping() };
};

AbstractTreeViewController::AbstractTreeViewController(const char * const context
	, std::shared_ptr<ISettings> settings
	, std::shared_ptr<DataProvider> dataProvider
)
	: m_context(context)
	, m_settings(std::move(settings))
	, m_dataProvider(std::move(dataProvider))
	, m_impl(*this)
{
	m_settings->RegisterObserver(m_impl.get());
}

AbstractTreeViewController::~AbstractTreeViewController()
{
	m_settings->UnregisterObserver(m_impl.get());
}

const char * AbstractTreeViewController::TrContext() const noexcept
{
	return m_context;
}

int AbstractTreeViewController::GetModeIndex() const
{
	return GetModeIndex(m_settings->Get(m_impl->settingsModeKey));
}

void AbstractTreeViewController::RegisterObserver(IObserver * observer)
{
	m_impl->Register(observer);
}

void AbstractTreeViewController::UnregisterObserver(IObserver * observer)
{
	m_impl->Unregister(observer);
}

void AbstractTreeViewController::Setup()
{
	OnModeChanged(m_settings->Get(m_impl->settingsModeKey));
}

void AbstractTreeViewController::SetMode(int index)
{
	m_impl->Perform(&IObserver::OnModeChanged, index);
}

void AbstractTreeViewController::SetMode(const QString & mode)
{
	m_settings->Set(m_impl->settingsModeKey, mode);
}
