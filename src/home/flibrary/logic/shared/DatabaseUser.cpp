#include "DatabaseUser.h"

#include <QtGui/QCursor>
#include <QtGui/QGuiApplication>

#include <plog/Log.h>

#include "data/DataItem.h"
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
	std::unique_ptr<Util::IExecutor> m_executor;

	Impl(const ILogicFactory & logicFactory
		, std::shared_ptr<DatabaseController> databaseController
	)
		: databaseController(std::move(databaseController))
		, m_executor(CreateExecutor(logicFactory))
	{
	}

	static std::unique_ptr<Util::IExecutor> CreateExecutor(const ILogicFactory & logicFactory)
	{
		return logicFactory.GetExecutor({
			  [] { }
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

std::unique_ptr<QTimer> DatabaseUser::CreateTimer(std::function<void()> f)
{
	auto timer = std::make_unique<QTimer>();
	timer->setSingleShot(true);
	timer->setInterval(std::chrono::milliseconds(200));
	QObject::connect(timer.get(), &QTimer::timeout, timer.get(), [f = std::move(f)]
	{
		f();
	});

	return timer;
};

size_t DatabaseUser::Execute(Util::IExecutor::Task && task, const int priority) const
{
	return (*m_impl->m_executor)(std::move(task), priority);
}

std::shared_ptr<DB::IDatabase> DatabaseUser::Database() const
{
	return m_impl->databaseController->GetDatabase();
}

IDataItem::Ptr DatabaseUser::CreateSimpleListItem(const DB::IQuery & query, const int * index)
{
	auto item = IDataItem::Ptr(NavigationItem::Create());

	item->SetId(query.Get<const char *>(index[0]));
	item->SetData(query.Get<const char *>(index[1]));

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