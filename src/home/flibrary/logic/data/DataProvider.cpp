#include "DataProvider.h"

#include <QString>

#include "Types.h"

using namespace HomeCompa::Flibrary;

class DataProvider::Impl
{
public:
	explicit Impl(std::shared_ptr<ILogicFactory> logicFactory)
		: m_logicFactory(std::move(logicFactory))
	{
	}

	void SetNavigationMode(const NavigationMode navigationMode)
	{
		m_navigationMode = navigationMode;
	}

	void SetViewMode(const ViewMode viewMode)
	{
		m_viewMode = viewMode;
	}

private:
	PropagateConstPtr<ILogicFactory, std::shared_ptr> m_logicFactory;
	NavigationMode m_navigationMode { NavigationMode::Unknown };
	ViewMode m_viewMode { ViewMode::Unknown };
};

DataProvider::DataProvider(std::shared_ptr<ILogicFactory> logicFactory)
	: m_impl(std::move(logicFactory))
{
}

DataProvider::~DataProvider() = default;

void DataProvider::SetNavigationMode(const NavigationMode navigationMode)
{
	m_impl->SetNavigationMode(navigationMode);
}

void DataProvider::SetViewMode(const ViewMode viewMode)
{
	m_impl->SetViewMode(viewMode);
}
