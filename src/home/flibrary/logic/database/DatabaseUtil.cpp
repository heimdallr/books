#include "DatabaseUtil.h"

#include <QHash>

#include <format>

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITemporaryTable.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/GenresLocalization.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IProgressController.h"

#include "data/DataItem.h"
#include "inpx/src/util/constant.h"
#include "util/localization.h"

#include "log.h"

namespace HomeCompa::Flibrary::DatabaseUtil
{

namespace
{

constexpr std::pair<int, int> BOOK_QUERY_TO_DATA[] {
	{   BookQueryFields::BookTitle,      BookItem::Column::Title },
    { BookQueryFields::SeriesTitle,     BookItem::Column::Series },
    {   BookQueryFields::SeqNumber,  BookItem::Column::SeqNumber },
	{	  BookQueryFields::Folder,     BookItem::Column::Folder },
    {    BookQueryFields::FileName,   BookItem::Column::FileName },
    {        BookQueryFields::Size,       BookItem::Column::Size },
	{     BookQueryFields::LibRate,    BookItem::Column::LibRate },
    {    BookQueryFields::UserRate,   BookItem::Column::UserRate },
    {  BookQueryFields::UpdateDate, BookItem::Column::UpdateDate },
	{		BookQueryFields::Lang,       BookItem::Column::Lang },
    {    BookQueryFields::FolderID,   BookItem::Column::FolderID },
    {    BookQueryFields::UpdateID,   BookItem::Column::UpdateID },
};

}

IDataItem::Ptr CreateSimpleListItem(const DB::IQuery& query, const size_t* index, const size_t removedIndex)
{
	auto item = IDataItem::Ptr(NavigationItem::Create());

	item->SetId(query.Get<const char*>(index[0]));
	item->SetData(query.Get<const char*>(index[1]));

	if (removedIndex)
		item->SetRemoved(query.Get<int>(removedIndex));

	return item;
}

IDataItem::Ptr CreateSeriesItem(const DB::IQuery& query, const size_t* index, const size_t removedIndex)
{
	auto item = IDataItem::Ptr(SeriesItem::Create());

	item->SetId(query.Get<const char*>(index[0]));
	item->SetData(query.Get<const char*>(index[1]), SeriesItem::Column::Title);
	item->SetData(query.Get<const char*>(index[2]), SeriesItem::Column::SeqNum);

	if (removedIndex)
		item->SetRemoved(query.Get<int>(removedIndex));

	return item;
}

IDataItem::Ptr CreateGenreItem(const DB::IQuery& query, const size_t* index, const size_t removedIndex)
{
	auto item = IDataItem::Ptr(GenreItem::Create());

	item->SetId(query.Get<const char*>(index[0]));
	if (removedIndex)
		item->SetRemoved(query.Get<int>(removedIndex));

	const auto* fbCode = query.Get<const char*>(index[2]);
	const auto translated = Loc::Tr(GENRE, fbCode);

	item->SetData(fbCode, GenreItem::Column::Fb2Code);
	item->SetData(translated != fbCode ? translated : query.Get<const char*>(index[1]));

	return item;
}

IDataItem::Ptr CreateLanguageItem(const DB::IQuery& query, const size_t* index, const size_t removedIndex)
{
	static const std::unordered_map<QString, const char*> languages { std::cbegin(LANGUAGES), std::cend(LANGUAGES) };

	auto item = IDataItem::Ptr(NavigationItem::Create());

	item->SetId(query.Get<const char*>(index[0]));
	if (removedIndex)
		item->SetRemoved(query.Get<int>(removedIndex));

	QString language = query.Get<const char*>(index[1]);
	const auto it = languages.find(language);

	item->SetData(it != languages.end() ? Loc::Tr(LANGUAGES_CONTEXT, it->second) : std::move(language));

	return item;
}

IDataItem::Ptr CreateFullAuthorItem(const DB::IQuery& query, const size_t* index, const size_t removedIndex)
{
	auto item = AuthorItem::Create();

	item->SetId(QString::number(query.Get<long long>(index[0])));
	if (removedIndex)
		item->SetRemoved(query.Get<int>(removedIndex));

	for (int i = 0; i < AuthorItem::Column::Last; ++i)
		item->SetData(query.Get<const char*>(index[i + 1]), i);

	return item;
}

IDataItem::Ptr CreateBookItem(const DB::IQuery& query)
{
	auto item = BookItem::Create();

	item->SetId(QString::number(query.Get<long long>(BookQueryFields::BookId)));
	for (const auto& [queryIndex, dataIndex] : BOOK_QUERY_TO_DATA)
		item->SetData(query.Get<const char*>(queryIndex), dataIndex);

	item->SetRemoved(query.Get<int>(BookQueryFields::IsDeleted));

	return item;
}

bool ChangeBookRemoved(DB::IDatabase& db, const std::unordered_set<long long>& ids, const bool remove, const std::shared_ptr<IProgressController>& progressController)
{
	auto progressItem = progressController ? progressController->Add(static_cast<int64_t>(11 * ids.size() / 10)) : std::make_unique<IProgressController::ProgressItemStub>();
	bool ok = true;
	const auto transaction = db.CreateTransaction();
	const auto tempTable = transaction->CreateTemporaryTable();
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
