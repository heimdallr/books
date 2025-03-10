#include "DatabaseUtil.h"

#include <QHash>

#include "database/interface/IQuery.h"

#include "interface/constants/GenresLocalization.h"
#include "interface/logic/IDatabaseUser.h"

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

} // namespace HomeCompa::Flibrary::DatabaseUtil
