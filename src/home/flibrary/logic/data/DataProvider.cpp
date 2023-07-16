#include "DataProvider.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QCursor>

#include <database/interface/IDatabase.h>

#include "util/executor/factory.h"
#include "util/IExecutor.h"

#include "interface/logic/ILogicFactory.h"

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

	void RequestNavigation(std::function<void(DataItem::Ptr)> callback)
	{
		(*m_executor)({"Get navigation", [&, callback = std::move(callback), mode = m_navigationMode] () mutable
		{
			return [&, mode, callback = std::move(callback)](const size_t)
			{
				if (mode == m_navigationMode)
					callback(DataItem::Ptr());
			};
		}}, 1);
	}

private:
	std::unique_ptr<Util::IExecutor> CreateExecutor()
	{
		const auto createDatabase = [this]
		{
			m_db.reset(m_logicFactory->GetDatabase());
//			emit m_self.OpenedChanged();
//			m_db->RegisterObserver(this);
		};

		const auto destroyDatabase = [this]
		{
//			m_db->UnregisterObserver(this);
			m_db.reset();
//			emit m_self.OpenedChanged();
		};

		return m_logicFactory->GetExecutor({
			  [=] { createDatabase(); }
			, [ ] { QGuiApplication::setOverrideCursor(Qt::BusyCursor); }
			, [ ] { QGuiApplication::restoreOverrideCursor(); }
			, [=] { destroyDatabase(); }
		});
	}

private:
	PropagateConstPtr<ILogicFactory, std::shared_ptr> m_logicFactory;
	PropagateConstPtr<DB::IDatabase> m_db { std::unique_ptr<DB::IDatabase>() };
	PropagateConstPtr<Util::IExecutor> m_executor{ CreateExecutor() };
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

void DataProvider::RequestNavigation(std::function<void(DataItem::Ptr)> callback)
{
	m_impl->RequestNavigation(std::move(callback));
}
