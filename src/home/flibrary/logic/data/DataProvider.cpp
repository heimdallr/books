#include "DataProvider.h"

#include <unordered_map>
#include <stack>

#include <QtGui/QGuiApplication>
#include <QtGui/QCursor>

#include <database/interface/IDatabase.h>

#include "database/interface/IQuery.h"
#include "util/executor/factory.h"
#include "util/IExecutor.h"

#include "interface/logic/ILogicFactory.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto AUTHORS_QUERY = "select AuthorID, FirstName, LastName, MiddleName from Authors";
constexpr auto SERIES_QUERY = "select SeriesID, SeriesTitle from Series";
constexpr auto GENRES_QUERY = "select GenreCode, ParentCode, GenreAlias from Genres";
constexpr auto GROUPS_QUERY = "select GroupID, Title from Groups_User";
constexpr auto ARCHIVES_QUERY = "select distinct Folder from Books";

}

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
				const auto query = m_db->CreateQuery(GENRES_QUERY);

				std::unordered_multimap<QString, DataItem::Ptr> items;
				std::unordered_map<QString, DataItem*> index;
				DataItem::Ptr root(NavigationItem::create());
				index.emplace("0", root.get());

				for (query->Execute(); !query->Eof(); query->Next())
				{
					auto & item = *items.emplace(query->Get<const char *>(1), DataItem::Ptr(NavigationItem::create()))->second->To<NavigationItem>();
					item.id = query->Get<const char *>(0);
					item.title = query->Get<const char *>(2);
					index.emplace(item.id, &item);
				}

				std::stack<QString> stack { {"0"} };

				while(!stack.empty())
				{
					auto parentId = std::move(stack.top());
					stack.pop();

					auto* parent = [&]
					{
						const auto it = index.find(parentId);
						assert(it != index.end());
						return it->second;
					}();

					for (auto && [it, end] = items.equal_range(parentId); it != end; ++it)
					{
						auto & item = parent->children.emplace_back(std::move(it->second));
						item->parent = parent;
						stack.push(item->To<NavigationItem>()->id);
					}
				}

				if (mode == m_navigationMode)
					callback(std::move(root));
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
