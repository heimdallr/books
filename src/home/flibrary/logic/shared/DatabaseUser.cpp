#include "DatabaseUser.h"

#include <QtGui/QCursor>
#include <QtGui/QGuiApplication>

#include <plog/Log.h>

#include "data/DataItem.h"
#include "data/GenresLocalization.h"
#include "interface/constants/Localization.h"
#include "shared/DatabaseController.h"
#include "util/executor/factory.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr std::pair<int, int> BOOK_QUERY_TO_DATA[]
{
	{ BookQueryFields::BookTitle, BookItem::Column::Title },
	{ BookQueryFields::SeriesTitle, BookItem::Column::Series },
	{ BookQueryFields::SeqNumber, BookItem::Column::SeqNumber },
	{ BookQueryFields::Folder, BookItem::Column::Folder },
	{ BookQueryFields::FileName, BookItem::Column::FileName },
	{ BookQueryFields::Size, BookItem::Column::Size },
	{ BookQueryFields::LibRate, BookItem::Column::LibRate },
	{ BookQueryFields::UpdateDate, BookItem::Column::UpdateDate },
	{ BookQueryFields::Lang, BookItem::Column::Lang },
};

}


struct DatabaseUser::Impl
{
	PropagateConstPtr<DatabaseController, std::shared_ptr> databaseController;
	std::shared_ptr<Util::IExecutor> executor;

	Impl(const ILogicFactory & logicFactory
		, std::shared_ptr<DatabaseController> databaseController
	)
		: databaseController(std::move(databaseController))
		, executor(CreateExecutor(logicFactory))
	{
	}

	static std::unique_ptr<Util::IExecutor> CreateExecutor(const ILogicFactory & logicFactory)
	{
		return logicFactory.GetExecutor({1
			, [] { }
			, [] { QGuiApplication::setOverrideCursor(Qt::BusyCursor); }
			, [] { QGuiApplication::restoreOverrideCursor(); }
			, [] { }
		});
	}
};

DatabaseUser::DatabaseUser(const std::shared_ptr<ILogicFactory> & logicFactory
	, std::shared_ptr<DatabaseController> databaseController
)
	: m_impl(*logicFactory, std::move(databaseController))
{
	PLOGD << "DatabaseUser created";
}

DatabaseUser::~DatabaseUser()
{
	PLOGD << "DatabaseUser destroyed";
}

size_t DatabaseUser::Execute(Util::IExecutor::Task && task, const int priority) const
{
	return (*m_impl->executor)(std::move(task), priority);
}

std::shared_ptr<DB::IDatabase> DatabaseUser::Database() const
{
	return m_impl->databaseController->GetDatabase();
}

std::shared_ptr<Util::IExecutor> DatabaseUser::Executor() const
{
	return m_impl->executor;
}

IDataItem::Ptr DatabaseUser::CreateSimpleListItem(const DB::IQuery & query, const int * index)
{
	auto item = IDataItem::Ptr(NavigationItem::Create());

	item->SetId(query.Get<const char *>(index[0]));
	item->SetData(query.Get<const char *>(index[1]));

	return item;
}

IDataItem::Ptr DatabaseUser::CreateGenreItem(const DB::IQuery & query, const int * index)
{
	auto item = IDataItem::Ptr(NavigationItem::Create());

	item->SetId(query.Get<const char *>(index[0]));

	const auto * fbCode = query.Get<const char *>(index[2]);
	const auto translated = Loc::Tr(GENRE, fbCode);
	item->SetData(translated != fbCode ? translated : query.Get<const char *>(index[1]));

	return item;
}

IDataItem::Ptr DatabaseUser::CreateFullAuthorItem(const DB::IQuery & query, const int * index)
{
	auto item = AuthorItem::Create();

	item->SetId(QString::number(query.Get<long long>(index[0])));
	for (int i = 0; i < AuthorItem::Column::Last; ++i)
		item->SetData(query.Get<const char *>(index[i + 1]), i);

	return item;
}

IDataItem::Ptr DatabaseUser::CreateBookItem(const DB::IQuery & query)
{
	auto item = BookItem::Create();

	item->SetId(QString::number(query.Get<long long>(BookQueryFields::BookId)));
	for (const auto & [queryIndex, dataIndex] : BOOK_QUERY_TO_DATA)
		item->SetData(query.Get<const char *>(queryIndex), dataIndex);

	auto & typed = *item->To<BookItem>();
	typed.removed = query.Get<int>(BookQueryFields::IsDeleted);

	return item;
}