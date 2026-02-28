#include "DataProvider.h"

#include <QTimer>

#include "fnd/FindPair.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/Enums.h"

#include "util/UiTimer.h"

#include "BooksTreeGenerator.h"
#include "DataItem.h"
#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

struct BooksViewModeDescription
{
	IBooksRootGenerator::Generator  generator;
	QueryDescription::MappingGetter mapping;
};

constexpr std::pair<ViewMode, BooksViewModeDescription> BOOKS_GENERATORS[] {
	{ ViewMode::List, { &IBooksRootGenerator::GetList, &QueryDescription::GetListMapping } },
	{ ViewMode::Tree, { &IBooksRootGenerator::GetTree, &QueryDescription::GetTreeMapping } },
};

}

class DataProvider::Impl
{
public:
	Impl(
		std::shared_ptr<const ICollectionProvider>   collectionProvider,
		std::shared_ptr<const IDatabaseUser>         databaseUser,
		std::shared_ptr<const IFilterProvider>       filterProvider,
		std::shared_ptr<INavigationQueryExecutor>    navigationQueryExecutor,
		std::shared_ptr<IAuthorAnnotationController> authorAnnotationController
	)
		: m_collectionProvider { std::move(collectionProvider) }
		, m_databaseUser { std::move(databaseUser) }
		, m_filterProvider { std::move(filterProvider) }
		, m_navigationQueryExecutor { std::move(navigationQueryExecutor) }
		, m_authorAnnotationController { std::move(authorAnnotationController) }
	{
	}

	void SetNavigationMode(const NavigationMode navigationMode)
	{
		m_navigationMode = navigationMode;
	}

	void SetNavigationId(QString id, const bool force)
	{
		if (force)
			m_booksGenerator.reset();

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

	void RequestNavigation(const bool force) const
	{
		m_requestNavigationForce = force;
		m_navigationTimer->start();
	}

	void RequestBooks(const bool force) const
	{
		if (force)
			m_booksGenerator.reset();

		m_booksTimer->start();
	}

	const QString& GetNavigationID() const noexcept
	{
		return m_navigationId;
	}

	BookInfo GetBookInfo(const long long id) const
	{
		assert(m_booksGenerator);
		return m_booksGenerator->GetBookInfo(id);
	}

private:
	void RequestNavigationImpl()
	{
		m_navigationQueryExecutor->RequestNavigation(
			m_navigationMode,
			[&](const NavigationMode mode, IDataItem::Ptr root) {
				SendNavigationCallback(mode, std::move(root));
			},
			m_requestNavigationForce
		);
	}

	void RequestBooksImpl()
	{
		if (m_booksViewMode == ViewMode::Unknown)
			return;

		const auto booksGeneratorReady = m_booksGenerator && m_booksGenerator->GetNavigationMode() == m_navigationMode && m_booksGenerator->GetNavigationId() == m_navigationId;

		const auto& description                    = m_navigationQueryExecutor->GetQueryDescription(m_navigationMode);
		const auto& [booksGenerator, columnMapper] = FindSecond(BOOKS_GENERATORS, m_booksViewMode);

		if (booksGeneratorReady && m_booksGenerator->GetBooksViewMode() == m_booksViewMode)
			return SendBooksCallback(m_navigationId, m_booksGenerator->GetCached(), (description.*columnMapper)());

		m_databaseUser->Execute(
			{ "Get books",
		      [this,
		       navigationMode = m_navigationMode,
		       navigationId   = m_navigationId,
		       viewMode       = m_booksViewMode,
		       generator      = std::move(m_booksGenerator),
		       booksGeneratorReady,
		       &description,
		       &booksGenerator,
		       &columnMapper]() mutable {
				  QString authorName;
				  if (!booksGeneratorReady)
				  {
					  const auto& activeCollection = m_collectionProvider->GetActiveCollection();
					  const auto  db               = m_databaseUser->Database();
					  generator                    = std::make_unique<BooksTreeGenerator>(activeCollection, *db, navigationMode, navigationId, description, *m_filterProvider);

					  if (navigationMode == NavigationMode::Authors && !navigationId.isEmpty())
					  {
						  const auto query = db->CreateQuery(QString("select LastName || ' ' || FirstName || ' ' || MiddleName from Authors where AuthorID = %1").arg(navigationId).toStdString());
						  query->Execute();
						  assert(!query->Eof());
						  authorName = query->Get<const char*>(0);
					  }
				  }

				  generator->SetBooksViewMode(viewMode);
				  auto root = std::invoke(booksGenerator, *generator, std::cref(description));
				  return [this, navigationId = std::move(navigationId), root = std::move(root), generator = std::move(generator), authorName = std::move(authorName), &description, &columnMapper](
							 size_t
						 ) mutable {
					  m_booksGenerator = std::move(generator);
					  SendBooksCallback(navigationId, std::move(root), (description.*columnMapper)());
					  if (!authorName.isEmpty())
						  m_authorAnnotationController->SetAuthor(navigationId.toLongLong(), std::move(authorName));
				  };
			  } },
			2
		);
	}

	void SendNavigationCallback(const NavigationMode mode, IDataItem::Ptr root) const
	{
		if (mode == m_navigationMode)
			m_navigationRequestCallback(std::move(root));
	}

	void SendBooksCallback(const QString& id, IDataItem::Ptr root, const BookItem::Mapping& columnMapping) const
	{
		if (id != m_navigationId)
			return;

		BookItem::mapping = &columnMapping;
		m_booksRequestCallback(std::move(root));
	}

private:
	NavigationMode m_navigationMode { NavigationMode::Unknown };
	ViewMode       m_booksViewMode { ViewMode::Unknown };
	QString        m_navigationId;
	Callback       m_navigationRequestCallback;
	Callback       m_booksRequestCallback;

	mutable bool                                m_requestNavigationForce { false };
	mutable std::shared_ptr<BooksTreeGenerator> m_booksGenerator;

	std::shared_ptr<const ICollectionProvider>                      m_collectionProvider;
	std::shared_ptr<const IDatabaseUser>                            m_databaseUser;
	std::shared_ptr<const IFilterProvider>                          m_filterProvider;
	PropagateConstPtr<INavigationQueryExecutor, std::shared_ptr>    m_navigationQueryExecutor;
	PropagateConstPtr<IAuthorAnnotationController, std::shared_ptr> m_authorAnnotationController;
	std::unique_ptr<QTimer>                                         m_navigationTimer { Util::CreateUiTimer([&] {
        RequestNavigationImpl();
    }) };
	std::unique_ptr<QTimer> m_booksTimer { Util::CreateUiTimer([&] {
		RequestBooksImpl();
	}) };
};

DataProvider::DataProvider(
	std::shared_ptr<const ICollectionProvider>   collectionProvider,
	std::shared_ptr<const IDatabaseUser>         databaseUser,
	std::shared_ptr<const IFilterProvider>       filterProvider,
	std::shared_ptr<INavigationQueryExecutor>    navigationQueryExecutor,
	std::shared_ptr<IAuthorAnnotationController> authorAnnotationController
)
	: m_impl(std::move(collectionProvider), std::move(databaseUser), std::move(filterProvider), std::move(navigationQueryExecutor), std::move(authorAnnotationController))
{
	PLOGV << "DataProvider created";
}

DataProvider::~DataProvider()
{
	PLOGV << "DataProvider destroyed";
}

void DataProvider::SetNavigationMode(const NavigationMode navigationMode)
{
	m_impl->SetNavigationMode(navigationMode);
}

void DataProvider::SetNavigationId(QString id, const bool force)
{
	m_impl->SetNavigationId(std::move(id), force);
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

void DataProvider::RequestNavigation(const bool force) const
{
	m_impl->RequestNavigation(force);
}

void DataProvider::RequestBooks(const bool force) const
{
	m_impl->RequestBooks(force);
}

const QString& DataProvider::GetNavigationID() const noexcept
{
	return m_impl->GetNavigationID();
}

BookInfo DataProvider::GetBookInfo(const long long id) const
{
	return m_impl->GetBookInfo(id);
}
