#include "DataProvider.h"

// ReSharper disable once CppUnusedIncludeDirective
#include <QString> // for plog

#include <plog/Log.h>

#include "fnd/FindPair.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/logic/INavigationQueryExecutor.h"

#include "util/UiTimer.h"

#include "database/DatabaseUser.h"

#include "BooksTreeGenerator.h"
#include "DataItem.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

struct BooksViewModeDescription
{
	IBooksRootGenerator::Generator generator;
	QueryDescription::MappingGetter mapping;
};

constexpr std::pair<ViewMode, BooksViewModeDescription> BOOKS_GENERATORS[]
{
	{ ViewMode::List, { &IBooksRootGenerator::GetList, &QueryDescription::GetListMapping } },
	{ ViewMode::Tree, { &IBooksRootGenerator::GetTree, &QueryDescription::GetTreeMapping } },
};

}

class DataProvider::Impl
{
public:
	Impl(std::shared_ptr<const DatabaseUser> databaseUser
		, std::shared_ptr<INavigationQueryExecutor> navigationQueryExecutor
	)
		: m_databaseUser(std::move(databaseUser))
		, m_navigationQueryExecutor(std::move(navigationQueryExecutor))
	{
	}

	void SetNavigationMode(const NavigationMode navigationMode)
	{
		m_navigationMode = navigationMode;
	}

	void SetNavigationId(QString id)
	{
		m_navigationId = std::move(id);
		if (m_booksViewMode != ViewMode::Unknown)
			m_booksTimer->start();
	}

	void SetBooksViewMode(const ViewMode viewMode)
	{
		m_booksViewMode = viewMode;
		if (!m_navigationId.isEmpty())
			m_booksTimer->start();
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
		m_navigationTimer->start();
	}

	void RequestBooks(const bool force) const
	{
		if (force)
			m_booksGenerator.reset();

		m_booksTimer->start();
	}

	BookInfo GetBookInfo(const long long id) const
	{
		assert(m_booksGenerator);
		return m_booksGenerator->GetBookInfo(id);
	}

private:
	void RequestNavigationImpl() const
	{
		m_navigationQueryExecutor->RequestNavigation(m_navigationMode, [&] (const NavigationMode mode, IDataItem::Ptr root)
		{
			SendNavigationCallback(mode, std::move(root));
		});
	}

	void RequestBooksImpl() const
	{
		if (m_booksViewMode == ViewMode::Unknown)
			return;

		const auto booksGeneratorReady = m_booksGenerator
			&& m_booksGenerator->GetNavigationMode() == m_navigationMode
			&& m_booksGenerator->GetNavigationId() == m_navigationId
			;

		const auto & description = m_navigationQueryExecutor->GetQueryDescription(m_navigationMode);
		const auto & [booksGenerator, columnMapper] = FindSecond(BOOKS_GENERATORS, m_booksViewMode);

		if (booksGeneratorReady && m_booksGenerator->GetBooksViewMode() == m_booksViewMode)
			return SendBooksCallback(m_navigationId, m_booksGenerator->GetCached(), (description.*columnMapper)());

		m_databaseUser->Execute({ "Get books",[this
			, navigationMode = m_navigationMode
			, navigationId = m_navigationId
			, viewMode = m_booksViewMode
			, generator = std::move(m_booksGenerator)
			, booksGeneratorReady
			, &description
			, &booksGenerator
			, &columnMapper
		] () mutable
		{
			if (!booksGeneratorReady)
			{
				const auto db = m_databaseUser->Database();
				generator = std::make_unique<BooksTreeGenerator>(*db, navigationMode, navigationId, description);
			}

			generator->SetBooksViewMode(viewMode);
			auto root = (*generator.*booksGenerator)(description.treeCreator);
			return [this
				, navigationId = std::move(navigationId)
				, root = std::move(root)
				, generator = std::move(generator)
				, &description
				, &columnMapper] (size_t) mutable
			{
				m_booksGenerator = std::move(generator);
				SendBooksCallback(navigationId, std::move(root), (description.*columnMapper)());
			};
		} }, 2);
	}

	void SendNavigationCallback(const NavigationMode mode, IDataItem::Ptr root) const
	{
		if (mode == m_navigationMode)
			m_navigationRequestCallback(std::move(root));
	}

	void SendBooksCallback(const QString & id, IDataItem::Ptr root, const BookItem::Mapping & columnMapping) const
	{
		if (id != m_navigationId)
			return;

		BookItem::mapping = &columnMapping;
		m_booksRequestCallback(std::move(root));
	}

private:
	NavigationMode m_navigationMode { NavigationMode::Unknown };
	ViewMode m_booksViewMode { ViewMode::Unknown };
	QString m_navigationId;
	Callback m_navigationRequestCallback;
	Callback m_booksRequestCallback;

	mutable std::shared_ptr<BooksTreeGenerator> m_booksGenerator;
	std::shared_ptr<const DatabaseUser> m_databaseUser;
	PropagateConstPtr<INavigationQueryExecutor, std::shared_ptr> m_navigationQueryExecutor;
	std::unique_ptr<QTimer> m_navigationTimer { Util::CreateUiTimer([&] { RequestNavigationImpl(); }) };
	std::unique_ptr<QTimer> m_booksTimer { Util::CreateUiTimer([&] { RequestBooksImpl(); }) };
};

DataProvider::DataProvider(std::shared_ptr<DatabaseUser> databaseUser
	, std::shared_ptr<INavigationQueryExecutor> navigationQueryExecutor
)
	: m_impl(std::move(databaseUser), std::move(navigationQueryExecutor))
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

void DataProvider::SetNavigationId(QString id)
{
	m_impl->SetNavigationId(std::move(id));
}

void DataProvider::SetBooksViewMode(const ViewMode viewMode)
{
	m_impl->SetBooksViewMode(viewMode);
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

void DataProvider::RequestBooks(const bool force) const
{
	m_impl->RequestBooks(force);
}

BookInfo DataProvider::GetBookInfo(const long long id) const
{
	return m_impl->GetBookInfo(id);
}
