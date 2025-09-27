#include "DatabaseUtil.h"

#include <format>

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITemporaryTable.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/GenresLocalization.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IProgressController.h"

#include "data/BooksTreeGenerator.h"
#include "data/DataItem.h"
#include "inpx/src/util/constant.h"
#include "util/localization.h"

#include "log.h"

namespace HomeCompa::Flibrary::DatabaseUtil
{

namespace
{

constexpr std::pair<int, int> BOOK_QUERY_TO_DATA[] {
	{  BookQueryFields::BookTitle,      BookItem::Column::Title },
    {  BookQueryFields::SeqNumber,  BookItem::Column::SeqNumber },
    { BookQueryFields::UpdateDate, BookItem::Column::UpdateDate },
	{    BookQueryFields::LibRate,    BookItem::Column::LibRate },
    {       BookQueryFields::Lang,       BookItem::Column::Lang },
    {       BookQueryFields::Year,       BookItem::Column::Year },
	{     BookQueryFields::Folder,     BookItem::Column::Folder },
    {   BookQueryFields::FileName,   BookItem::Column::FileName },
    {       BookQueryFields::Size,       BookItem::Column::Size },
	{   BookQueryFields::UserRate,   BookItem::Column::UserRate },
    {      BookQueryFields::LibID,      BookItem::Column::LibID },
};

void UpdateItem(IDataItem& item, const DB::IQuery& query, const std::initializer_list<const size_t>& index, const size_t removedIndex, const size_t flagsIndex)
{
	item.SetId(query.Get<const char*>(0));
	for (int column = 0; const auto i : index)
		item.SetData(query.Get<const char*>(i), column++);

	if (query.ColumnCount() > removedIndex)
		item.SetRemoved(query.Get<int>(removedIndex));

	if (query.ColumnCount() > flagsIndex)
		item.SetFlags(static_cast<IDataItem::Flags>(query.Get<int>(flagsIndex)));
}

}

IDataItem::Ptr CreateSimpleListItem(const DB::IQuery& query)
{
	auto item = IDataItem::Ptr(NavigationItem::Create());
	UpdateItem(*item, query, { 1 }, 2, 3);
	return item;
}

IDataItem::Ptr CreateSeriesItem(const DB::IQuery& query)
{
	auto item = IDataItem::Ptr(SeriesItem::Create());
	UpdateItem(*item, query, { 1, 2 }, 3, 4);
	return item;
}

IDataItem::Ptr CreateGenreItem(const DB::IQuery& query)
{
	auto item = IDataItem::Ptr(GenreItem::Create());
	UpdateItem(*item, query, {}, 3, 4);

	const auto* fbCode = query.Get<const char*>(2);
	const auto translated = Loc::Tr(GENRE, fbCode);

	item->SetData(fbCode, GenreItem::Column::Fb2Code);
	item->SetData(translated != fbCode ? translated : query.Get<const char*>(1));

	return item;
}

IDataItem::Ptr CreateLanguageItem(const DB::IQuery& query)
{
	static const auto languages = GetLanguagesMap();

	auto item = IDataItem::Ptr(NavigationItem::Create());
	UpdateItem(*item, query, {}, 1, 2);

	const auto it = languages.find(item->GetId());

	item->SetData(it != languages.end() ? Loc::Tr(LANGUAGES_CONTEXT, it->second) : item->GetId());

	return item;
}

IDataItem::Ptr CreateFullAuthorItem(const DB::IQuery& query)
{
	auto item = AuthorItem::Create();
	UpdateItem(*item, query, { 1, 1, 2, 3 }, 4, 5);
	return item;
}

IDataItem::Ptr CreateBookItem(const DB::IQuery& query)
{
	auto item = BookItem::Create();

	item->SetId(QString::number(query.Get<long long>(BookQueryFields::BookId)));
	for (const auto& [queryIndex, dataIndex] : BOOK_QUERY_TO_DATA)
		item->SetData(query.Get<const char*>(queryIndex), dataIndex);

	if (const auto flags = static_cast<IDataItem::Flags>(query.Get<int>(BookQueryFields::Flags)); !!(flags & IDataItem::Flags::BooksFiltered))
		item->SetFlags(IDataItem::Flags::Filtered);
	item->SetRemoved(query.Get<int>(BookQueryFields::IsDeleted));

	return item;
}

bool ChangeBookRemoved(DB::IDatabase& db, const std::unordered_set<long long>& ids, const bool remove, const std::shared_ptr<IProgressController>& progressController)
{
	auto progressItem = progressController ? progressController->Add(static_cast<int64_t>(11 * ids.size() / 10)) : std::make_unique<IProgressController::ProgressItemStub>();
	bool ok = true;
	const auto tempTable = db.CreateTemporaryTable();
	const auto transaction = db.CreateTransaction();
	{
		const auto command = transaction->CreateCommand(std::format("insert into {} (id) values (?)", tempTable->GetName()));
		for (const auto id : ids)
		{
			command->Bind(0, id);
			ok = command->Execute() && ok;
			progressItem->Increment(1);
		}
	}
	transaction
		->CreateCommand(std::format("insert or replace into Books_User(BookID, IsDeleted, CreatedAt) select id, {}, datetime(CURRENT_TIMESTAMP, 'localtime') from {}", remove ? 1 : 0, tempTable->GetName()))
		->Execute();

	ok = transaction
	         ->CreateCommand(R"(
		delete from Books_User
			where UserRate is null and Lang is null
			and exists (select 42 from Books where Books.BookID = Books_User.BookID and Books.IsDeleted = Books_User.IsDeleted)
	)")
	         ->Execute()
	  && ok;

	for (const auto& [table, where, byBooks, join, additional] : IS_DELETED_UPDATE_ARGS)
	{
		PLOGV << "set IsDeleted for " << table;
		ok = transaction
		         ->CreateCommand(QString("%1%2")
		                             .arg(QString(IS_DELETED_UPDATE_STATEMENT_TOTAL).arg(table, join, where, additional))
		                             .arg(QString(IS_DELETED_UPDATE_STATEMENT_BY_BOOKS).arg(tempTable->GetName().data(), QString(byBooks).arg(where)))
		                             .toStdString())
		         ->Execute()
		  && ok;
	}

	return transaction->Commit() && ok;
}

} // namespace HomeCompa::Flibrary::DatabaseUtil
