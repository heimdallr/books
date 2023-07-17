#include "DataProvider.h"

#include <unordered_map>
#include <stack>

#include <QtGui/QGuiApplication>
#include <QtGui/QCursor>

#include "fnd/FindPair.h"

#include <database/interface/IDatabase.h>
#include <plog/Log.h>

#include "database/interface/IQuery.h"
#include "util/executor/factory.h"
#include "util/IExecutor.h"

#include "interface/logic/ILogicFactory.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto AUTHORS_QUERY = "select AuthorID, FirstName, LastName, MiddleName from Authors";
constexpr auto SERIES_QUERY = "select SeriesID, SeriesTitle from Series";
constexpr auto GENRES_QUERY = "select GenreCode, GenreAlias, ParentCode from Genres";
constexpr auto GROUPS_QUERY = "select GroupID, Title from Groups_User";
constexpr auto ARCHIVES_QUERY = "select distinct Folder from Books";

using QueryDataExtractor = DataItem::Ptr(*)(const DB::IQuery & query);

struct QueryDescription
{
	const char * query = nullptr;
	QueryDataExtractor extractor = nullptr;
};

class IQueryExecutor  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IQueryExecutor() = default;
	virtual void RequestSimpleList(QueryDescription queryDescription, DataProvider::Callback callback) const = 0;
	virtual void RequestGenres(QueryDescription queryDescription, DataProvider::Callback callback) const = 0;
	virtual void Stub(QueryDescription, DataProvider::Callback) const {}
};

using QueryExecutorFunctor = void(IQueryExecutor::*)(QueryDescription, DataProvider::Callback) const;

DataItem::Ptr SimpleListItem(const DB::IQuery & query)
{
	auto item = DataItem::Ptr(NavigationItem::Create());
	auto & types = *item->To<NavigationItem>();
	types.id = query.Get<const char *>(0);
	types.title = query.Get<const char *>(1);
	return item;
}

DataItem::Ptr IdOnly(const DB::IQuery & query)
{
	auto item = DataItem::Ptr(NavigationItem::Create());
	auto & types = *item->To<NavigationItem>();
	types.title = types.id = query.Get<const char *>(0);
	return item;
}

void AppendTitle(QString & title, std::string_view str)
{
	if (title.isEmpty())
	{
		title = str.data();
		return;
	}

	if (!str.empty())
		title.append(" ").append(str.data());
}

QString CreateAuthorTitle(const DB::IQuery & query)
{
	QString title = query.Get<const char *>(2);
	AppendTitle(title, query.Get<const char *>(1));
	AppendTitle(title, query.Get<const char *>(3));

//	if (title.isEmpty())
//		title = QCoreApplication::translate(Constant::Localization::CONTEXT_ERROR, Constant::Localization::AUTHOR_NOT_SPECIFIED);

	return title;
}

DataItem::Ptr AuthorItem(const DB::IQuery & query)
{
	auto item = DataItem::Ptr(NavigationItem::Create());
	auto & types = *item->To<NavigationItem>();
	types.id = query.Get<const char *>(0);
	types.title = CreateAuthorTitle(query);
	return item;
}

constexpr std::pair<NavigationMode, std::pair<QueryExecutorFunctor, QueryDescription>> QUERIES[]
{
	{ NavigationMode::Authors , { &IQueryExecutor::RequestSimpleList, { AUTHORS_QUERY , &AuthorItem}}},
	{ NavigationMode::Series  , { &IQueryExecutor::RequestSimpleList, { SERIES_QUERY  , &SimpleListItem}}},
	{ NavigationMode::Genres  , { &IQueryExecutor::RequestGenres    , { GENRES_QUERY  , &SimpleListItem}}},
	{ NavigationMode::Groups  , { &IQueryExecutor::RequestSimpleList, { GROUPS_QUERY  , &SimpleListItem}}},
	{ NavigationMode::Archives, { &IQueryExecutor::RequestSimpleList, { ARCHIVES_QUERY, &IdOnly}}},
};

}

class DataProvider::Impl
	: virtual public IQueryExecutor
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

	void RequestNavigation(Callback callback) const
	{
		const auto [functor, description] = FindSecond(QUERIES, m_navigationMode);
		std::invoke(functor, this, description, std::move(callback));
	}

private: // IQueryExecutor
	void RequestSimpleList(QueryDescription queryDescription, Callback callback) const override
	{
		(*m_executor)({ "Get navigation", [&db = *m_db, &modeRef = m_navigationMode, mode = m_navigationMode, callback = std::move(callback), queryDescription] () mutable
		{
			DataItem::Ptr root(NavigationItem::Create());
			const auto query = db.CreateQuery(queryDescription.query);

			for (query->Execute(); !query->Eof(); query->Next())
				root->AppendChild(queryDescription.extractor(*query));

			return [&modeRef, mode, callback = std::move(callback), root = std::move(root)] (size_t) mutable
			{
				if (mode == modeRef)
					callback(std::move(root));
			};
		} }, 1);
	}

	void RequestGenres(QueryDescription queryDescription, Callback callback) const override
	{
		(*m_executor)({"Get navigation", [&db = *m_db, &modeRef = m_navigationMode, mode = m_navigationMode, callback = std::move(callback), queryDescription] () mutable
		{
			std::unordered_multimap<QString, DataItem::Ptr> items;
			std::unordered_map<QString, DataItem *> index;

			DataItem::Ptr root(NavigationItem::Create());
			index.emplace("0", root.get());

			const auto query = db.CreateQuery(queryDescription.query);
			for (query->Execute(); !query->Eof(); query->Next())
			{
				auto & item = *items.emplace(query->Get<const char *>(2), queryDescription.extractor(*query))->second->To<NavigationItem>();
				index.emplace(item.id, &item);
			}

			std::stack<QString> stack { {"0"} };

			while (!stack.empty())
			{
				auto parentId = std::move(stack.top());
				stack.pop();

				auto * parent = [&]
				{
					const auto it = index.find(parentId);
					assert(it != index.end());
					return it->second;
				}();

				for (auto && [it, end] = items.equal_range(parentId); it != end; ++it)
				{
					const auto & item = parent->AppendChild(std::move(it->second));
					stack.push(item->To<NavigationItem>()->id);
				}
			}

			return [&modeRef, mode, callback = std::move(callback), root = std::move(root)] (size_t) mutable
			{
				if (mode == modeRef)
					callback(std::move(root));
			};
		}}, 1);
	}

private:
	std::unique_ptr<Util::IExecutor> CreateExecutor()
	{
		const auto createDatabase = [this]
		{
			m_db = m_logicFactory->GetDatabase();
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
	std::unique_ptr<DB::IDatabase> m_db;
	std::unique_ptr<Util::IExecutor> m_executor{ CreateExecutor() };
	NavigationMode m_navigationMode { NavigationMode::Unknown };
	ViewMode m_viewMode { ViewMode::Unknown };
};

DataProvider::DataProvider(std::shared_ptr<ILogicFactory> logicFactory)
	: m_impl(std::move(logicFactory))
{
	PLOGD << "DataProvider created";
}

DataProvider::~DataProvider()
{
	PLOGD << "DataProvider destroyed";
}

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
