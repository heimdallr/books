#include "DataProvider.h"

#include <stack>
#include <unordered_map>

#include <plog/Log.h>
#include <QtGui/QCursor>
#include <QtGui/QGuiApplication>

#include "fnd/FindPair.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "util/executor/factory.h"
#include "util/IExecutor.h"

#include "interface/constants/Localization.h"
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

class INavigationQueryExecutor  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~INavigationQueryExecutor() = default;
	virtual void RequestNavigationSimpleList(QueryDescription queryDescription) const = 0;
	virtual void RequestNavigationGenres(QueryDescription queryDescription) const = 0;
};

using QueryExecutorFunctor = void(INavigationQueryExecutor::*)(QueryDescription) const;

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

	if (title.isEmpty())
		title = QCoreApplication::translate(Constant::Localization::CONTEXT_ERROR, Constant::Localization::AUTHOR_NOT_SPECIFIED);

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
	{ NavigationMode::Authors , { &INavigationQueryExecutor::RequestNavigationSimpleList, { AUTHORS_QUERY , &AuthorItem}}},
	{ NavigationMode::Series  , { &INavigationQueryExecutor::RequestNavigationSimpleList, { SERIES_QUERY  , &SimpleListItem}}},
	{ NavigationMode::Genres  , { &INavigationQueryExecutor::RequestNavigationGenres    , { GENRES_QUERY  , &SimpleListItem}}},
	{ NavigationMode::Groups  , { &INavigationQueryExecutor::RequestNavigationSimpleList, { GROUPS_QUERY  , &SimpleListItem}}},
	{ NavigationMode::Archives, { &INavigationQueryExecutor::RequestNavigationSimpleList, { ARCHIVES_QUERY, &IdOnly}}},
};

}

class DataProvider::Impl
	: virtual public INavigationQueryExecutor
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

	void SetNavigationRequestCallback(Callback callback)
	{
		m_navigationRequestCallback = std::move(callback);
	}

	void SetBookRequestCallback(Callback callback)
	{
		m_booksRequestCallback = std::move(callback);
	}

	void RequestNavigation() const
	{
		const auto [functor, description] = FindSecond(QUERIES, m_navigationMode);
		std::invoke(functor, this, description);
	}

	void RequestBooks(QString /*id*/) const
	{
	}

private: // INavigationQueryExecutor
	void RequestNavigationSimpleList(QueryDescription queryDescription) const override
	{
		(*m_executor)({ "Get navigation", [&, &modeRef = m_navigationMode, mode = m_navigationMode, queryDescription] ()
		{
			DataItem::Ptr root(NavigationItem::Create());
			const auto query = m_db->CreateQuery(queryDescription.query);

			PLOGD << "Sort started";
			std::map<QString, DataItem::Ptr> data;
			for (query->Execute(); !query->Eof(); query->Next())
			{
				auto item = queryDescription.extractor(*query);
				auto title = item->To<NavigationItem>()->title.toLower();
				data.emplace_hint(data.end(), std::move(title), std::move(item));
			}
			PLOGD << "Sort started";

			for (auto& item : data | std::views::values)
				root->AppendChild(std::move(item));

			return [&, mode, root = std::move(root)] (size_t) mutable
			{
				if (mode == modeRef)
					m_navigationRequestCallback(std::move(root));
			};
		} }, 1);
	}

	void RequestNavigationGenres(QueryDescription queryDescription) const override
	{
		(*m_executor)({"Get navigation", [&, &modeRef = m_navigationMode, mode = m_navigationMode, queryDescription] ()
		{
			std::unordered_multimap<QString, DataItem::Ptr> items;
			std::unordered_map<QString, DataItem *> index;

			DataItem::Ptr root(NavigationItem::Create());
			index.emplace("0", root.get());

			const auto query = m_db->CreateQuery(queryDescription.query);
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

			return [&, mode, root = std::move(root)] (size_t) mutable
			{
				if (mode == modeRef)
					m_navigationRequestCallback(std::move(root));
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
	Callback m_navigationRequestCallback;
	Callback m_booksRequestCallback;
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

void DataProvider::SetNavigationRequestCallback(Callback callback)
{
	m_impl->SetNavigationRequestCallback(std::move(callback));
}

void DataProvider::SetBookRequestCallback(Callback callback)
{
	m_impl->SetBookRequestCallback(std::move(callback));
}

void DataProvider::RequestNavigation() const
{
	m_impl->RequestNavigation();
}

void DataProvider::RequestBooks(QString id) const
{
	m_impl->RequestBooks(std::move(id));
}
