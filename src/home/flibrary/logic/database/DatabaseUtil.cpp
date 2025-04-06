#include "DatabaseUtil.h"

#include <QHash>

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/GenresLocalization.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IProgressController.h"

#include "data/DataItem.h"
#include "util/localization.h"

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
};

}

IDataItem::Ptr CreateSimpleListItem(const DB::IQuery& query, const size_t* index)
{
	auto item = IDataItem::Ptr(NavigationItem::Create());

	item->SetId(query.Get<const char*>(index[0]));
	item->SetData(query.Get<const char*>(index[1]));

	return item;
}

IDataItem::Ptr CreateGenreItem(const DB::IQuery& query, const size_t* index)
{
	auto item = IDataItem::Ptr(GenreItem::Create());

	item->SetId(query.Get<const char*>(index[0]));

	const auto* fbCode = query.Get<const char*>(index[2]);
	const auto translated = Loc::Tr(GENRE, fbCode);

	item->SetData(fbCode, GenreItem::Column::Fb2Code);
	item->SetData(translated != fbCode ? translated : query.Get<const char*>(index[1]));

	return item;
}

IDataItem::Ptr CreateLanguageItem(const DB::IQuery& query, const size_t* index)
{
	static const std::unordered_map<QString, const char*> languages { std::cbegin(LANGUAGES), std::cend(LANGUAGES) };

	auto item = IDataItem::Ptr(NavigationItem::Create());

	item->SetId(query.Get<const char*>(index[0]));

	QString language = query.Get<const char*>(index[1]);
	const auto it = languages.find(language);

	item->SetData(it != languages.end() ? Loc::Tr(LANGUAGES_CONTEXT, it->second) : std::move(language));

	return item;
}

IDataItem::Ptr CreateFullAuthorItem(const DB::IQuery& query, const size_t* index)
{
	auto item = AuthorItem::Create();

	item->SetId(QString::number(query.Get<long long>(index[0])));
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

	auto& typed = *item->To<BookItem>();
	typed.removed = query.Get<int>(BookQueryFields::IsDeleted);

	return item;
}

bool ChangeBookRemoved(DB::IDatabase& db, const std::unordered_set<long long>& ids, const bool remove, const std::shared_ptr<IProgressController>& progressController)
{
	auto progressItem = progressController ? progressController->Add(static_cast<int64_t>(11 * ids.size() / 10)) : std::make_unique<IProgressController::ProgressItemStub>();
	bool ok = true;
	const auto transaction = db.CreateTransaction();
	{
		const auto command = transaction->CreateCommand("insert or replace into Books_User(BookID, IsDeleted, CreatedAt) values(:id, :is_deleted, datetime(CURRENT_TIMESTAMP, 'localtime'))");
		for (const auto id : ids)
		{
			command->Bind(":id", id);
			command->Bind(":is_deleted", remove ? 1 : 0);
			ok = command->Execute() && ok;
			progressItem->Increment(1);
		}
	}
	{
		ok = transaction
		         ->CreateCommand(R"(
			delete from Books_User 
			where UserRate is null 
			and exists (select 42 from Books where Books.BookID = Books_User.BookID and Books.IsDeleted = Books_User.IsDeleted)
		)")
		         ->Execute()
		  && ok;
	}

	return transaction->Commit() && ok;
}

} // namespace HomeCompa::Flibrary::DatabaseUtil
