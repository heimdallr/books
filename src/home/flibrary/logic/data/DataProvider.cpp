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

DataItem::Ptr CreateSimpleListItem(const DB::IQuery & query)
{
	auto item = DataItem::Ptr(NavigationItem::Create());
	auto & typed = *item->To<NavigationItem>();

	typed.id = query.Get<const char *>(0);
	typed.title = query.Get<const char *>(1);

	return item;
}

DataItem::Ptr CreateIdOnlyItem(const DB::IQuery & query)
{
	auto item = NavigationItem::Create();
	auto & typed = *item->To<NavigationItem>();

	typed.title = typed.id = query.Get<const char *>(0);

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

DataItem::Ptr CreateAuthorItem(const DB::IQuery & query)
{
	auto item = AuthorItem::Create();
	auto & typed = *item->To<AuthorItem>();

	typed.id = query.Get<const char *>(0);
	typed.title = query.Get<const char *>(2);
	typed.firstName = query.Get<const char *>(1);
	typed.middleName = query.Get<const char *>(3);

	return item;
}

constexpr std::pair<NavigationMode, std::pair<QueryExecutorFunctor, QueryDescription>> QUERIES[]
{
	{ NavigationMode::Authors , { &INavigationQueryExecutor::RequestNavigationSimpleList, { AUTHORS_QUERY , &CreateAuthorItem}}},
	{ NavigationMode::Series  , { &INavigationQueryExecutor::RequestNavigationSimpleList, { SERIES_QUERY  , &CreateSimpleListItem}}},
	{ NavigationMode::Genres  , { &INavigationQueryExecutor::RequestNavigationGenres    , { GENRES_QUERY  , &CreateSimpleListItem}}},
	{ NavigationMode::Groups  , { &INavigationQueryExecutor::RequestNavigationSimpleList, { GROUPS_QUERY  , &CreateSimpleListItem}}},
	{ NavigationMode::Archives, { &INavigationQueryExecutor::RequestNavigationSimpleList, { ARCHIVES_QUERY, &CreateIdOnlyItem}}},
};

}

class DataProvider::Impl
	: virtual public INavigationQueryExecutor
{
private:
	using Cache = std::unordered_map<QString, DataItem::Ptr>;

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
		(*m_executor)({ "Get navigation", [&, mode = m_navigationMode, queryDescription] ()
		{
			Cache cache;
			const auto query = m_db->CreateQuery(queryDescription.query);
			for (query->Execute(); !query->Eof(); query->Next())
			{
				auto item = queryDescription.extractor(*query);
				cache.emplace(item->GetId(), item);
			}

			DataItem::Items items;
			items.reserve(std::size(cache));
			std::ranges::copy(cache | std::views::values, std::back_inserter(items));

			std::ranges::sort(items, [] (const DataItem::Ptr & lhs, const DataItem::Ptr & rhs)
			{
				return QString::compare(lhs->To<NavigationItem>()->title, rhs->To<NavigationItem>()->title, Qt::CaseInsensitive) < 0;
			});

			DataItem::Ptr root(NavigationItem::Create());
			root->SetChildren(std::move(items));

			return [&, mode, root = std::move(root), cache = std::move(cache)] (size_t) mutable
			{
				SendNavigationCallback(mode, std::move(root), std::move(cache));
			};
		} }, 1);
	}

	void RequestNavigationGenres(QueryDescription queryDescription) const override
	{
		(*m_executor)({"Get navigation", [&, mode = m_navigationMode, queryDescription] ()
		{
			Cache cache;
			DataItem::Ptr root(NavigationItem::Create());
			std::unordered_multimap<QString, DataItem::Ptr> items;
			cache.emplace("0", root);

			const auto query = m_db->CreateQuery(queryDescription.query);
			for (query->Execute(); !query->Eof(); query->Next())
			{
				auto item = items.emplace(query->Get<const char *>(2), queryDescription.extractor(*query))->second;
				cache.emplace(item->GetId(), std::move(item));
			}

			std::stack<QString> stack { {"0"} };

			while (!stack.empty())
			{
				auto parentId = std::move(stack.top());
				stack.pop();

				const auto parent = [&]
				{
					const auto it = cache.find(parentId);
					assert(it != cache.end());
					return it->second;
				}();

				for (auto && [it, end] = items.equal_range(parentId); it != end; ++it)
				{
					const auto & item = parent->AppendChild(std::move(it->second));
					stack.push(item->To<NavigationItem>()->id);
				}
			}

			assert(std::ranges::all_of(items | std::views::values, [] (const auto & item) { return !item; }));

			return [&, mode, root = std::move(root), cache = std::move(cache)] (size_t) mutable
			{
				SendNavigationCallback(mode, std::move(root), std::move(cache));
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

	void SendNavigationCallback(const NavigationMode mode, DataItem::Ptr root, Cache cache) const
	{
		m_navigationCache[static_cast<int>(mode)] = std::move(cache);
		if (mode == m_navigationMode)
			m_navigationRequestCallback(std::move(root));
	}

private:
	PropagateConstPtr<ILogicFactory, std::shared_ptr> m_logicFactory;
	std::unique_ptr<DB::IDatabase> m_db;
	std::unique_ptr<Util::IExecutor> m_executor{ CreateExecutor() };
	NavigationMode m_navigationMode { NavigationMode::Unknown };
	ViewMode m_viewMode { ViewMode::Unknown };
	Callback m_navigationRequestCallback;
	Callback m_booksRequestCallback;

	mutable Cache m_navigationCache[static_cast<int>(NavigationMode::Last)];
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
