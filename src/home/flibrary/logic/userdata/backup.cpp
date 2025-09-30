#include "backup.h"

#include <QFile>
#include <QString>

#include "fnd/FindPair.h"
#include "fnd/ScopedCall.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/Localization.h"
#include "interface/constants/ProductConstant.h"
#include "interface/logic/IFilterProvider.h"

#include "constants/ExportStat.h"
#include "constants/books.h"
#include "constants/filter.h"
#include "constants/groups.h"
#include "constants/searches.h"
#include "util/IExecutor.h"
#include "util/xml/XmlAttributes.h"
#include "util/xml/XmlWriter.h"

#include "log.h"

namespace HomeCompa::Flibrary::UserData
{

namespace
{

constexpr auto CANNOT_WRITE = QT_TRANSLATE_NOOP("UserData", "Cannot write to '%1'");

using BackupFunction = void (*)(DB::IDatabase& db, Util::XmlWriter&);

class XmlAttributes final : public Util::XmlAttributes
{
public:
	template <std::ranges::input_range R>
	XmlAttributes(const R& r, DB::IQuery& query)
	{
		m_values.reserve(std::size(r));
		std::ranges::transform(r, std::back_inserter(m_values), [&, n = 0](const auto& value) mutable { return std::pair(value, query.Get<const char*>(n++)); });
	}

	XmlAttributes() = default;

	explicit XmlAttributes(std::vector<std::pair<QString, QString>> values)
		: m_values { std::move(values) }
	{
	}

private: // Util::XmlAttributes
	QString GetAttribute(const QString& key) const override
	{
		return FindSecond(m_values, key, s_empty);
	}

	size_t GetCount() const override
	{
		return std::size(m_values);
	}

	QString GetName(const size_t index) const override
	{
		assert(index < GetCount());
		return m_values[index].first;
	}

	QString GetValue(const size_t index) const override
	{
		assert(index < GetCount());
		return m_values[index].second;
	}

private:
	std::vector<std::pair<QString, QString>> m_values;
	const QString s_empty;
};

void BackupUserDataBooks(DB::IDatabase& db, Util::XmlWriter& xmlWriter)
{
	static constexpr auto text = "select f.FolderTitle, b.FileName, u.IsDeleted, u.UserRate, u.Lang, u.CreatedAt "
								 "from Books_User u "
								 "join Books b on b.BookID = u.BookID "
								 "join Folders f on f.FolderID = b.FolderID ";

	static constexpr const char* fields[] = {
		Constant::UserData::Books::Folder,   Constant::UserData::Books::FileName, Constant::UserData::Books::IsDeleted,
		Constant::UserData::Books::UserRate, Constant::UserData::Books::Lang,     Constant::UserData::Books::CreatedAt,
	};

	const auto query = db.CreateQuery(text);
	for (query->Execute(), assert(query->ColumnCount() == std::size(fields)); !query->Eof(); query->Next())
		xmlWriter.WriteStartElement(Constant::ITEM, XmlAttributes(fields, *query)).WriteEndElement();
}

void BackupUserDataGroups(DB::IDatabase& db, Util::XmlWriter& xmlWriter)
{
	static constexpr auto text = "select g.Title, g.CreatedAt, gl.CreatedAt, b.FileName, f.FolderTitle "
								 ", a.LastName||','||a.FirstName||','||a.MiddleName "
								 ", s.SeriesTitle, k.KeywordTitle "
								 "from Groups_User g "
								 "left join Groups_List_User gl on gl.GroupID = g.GroupID "
								 "left join Books b on b.BookID = gl.ObjectID "
								 "left join Folders f on f.FolderID = b.FolderID "
								 "left join Authors a on a.AuthorID = gl.ObjectID "
								 "left join Series s on s.SeriesID = gl.ObjectID "
								 "left join Keywords k on k.KeywordID = gl.ObjectID "
								 "order by g.GroupID ";

	std::unique_ptr<ScopedCall> group;
	QString currentTitle;
	const auto query = db.CreateQuery(text);
	for (query->Execute(); !query->Eof(); query->Next())
	{
		QString title = query->Get<const char*>(0);
		QString groupCreatedAt = query->Get<const char*>(1);
		if (currentTitle != title)
		{
			currentTitle = std::move(title);
			group.reset();
			std::make_unique<ScopedCall>(
				[&]
				{
					xmlWriter.WriteStartElement(Constant::UserData::Groups::GroupNode,
				                                XmlAttributes({
													{                      Constant::TITLE,   currentTitle },
													{ Constant::UserData::Books::CreatedAt, groupCreatedAt },
                    }));
				},
				[&] { xmlWriter.WriteEndElement(); })
				.swap(group);
		}

		if (const auto* fileName = query->Get<const char*>(3))
			xmlWriter.WriteStartElement(Constant::ITEM)
				.WriteAttribute(Constant::UserData::Books::CreatedAt, query->Get<const char*>(2))
				.WriteAttribute(Constant::UserData::Books::FileName, fileName)
				.WriteAttribute(Constant::UserData::Books::Folder, query->Get<const char*>(4))
				.WriteEndElement();
		else if (const auto* authorName = query->Get<const char*>(5))
			xmlWriter.WriteStartElement(Constant::UserData::Groups::Author)
				.WriteAttribute(Constant::UserData::Books::CreatedAt, query->Get<const char*>(2))
				.WriteAttribute(Constant::TITLE, authorName)
				.WriteEndElement();
		else if (const auto* seriesTitle = query->Get<const char*>(6))
			xmlWriter.WriteStartElement(Constant::UserData::Groups::Series)
				.WriteAttribute(Constant::UserData::Books::CreatedAt, query->Get<const char*>(2))
				.WriteAttribute(Constant::TITLE, seriesTitle)
				.WriteEndElement();
		else if (const auto* keywordTitle = query->Get<const char*>(7))
			xmlWriter.WriteStartElement(Constant::UserData::Groups::Keyword)
				.WriteAttribute(Constant::UserData::Books::CreatedAt, query->Get<const char*>(2))
				.WriteAttribute(Constant::TITLE, keywordTitle)
				.WriteEndElement();
	}
}

void BackupUserDataSearches(DB::IDatabase& db, Util::XmlWriter& xmlWriter)
{
	static constexpr auto text = "select s.Title, s.CreatedAt from Searches_User s ";

	static constexpr const char* fields[] = {
		Constant::TITLE,
		Constant::UserData::Books::CreatedAt,
	};

	const auto query = db.CreateQuery(text);
	for (query->Execute(); !query->Eof(); query->Next())
		xmlWriter.WriteStartElement(Constant::ITEM, XmlAttributes(fields, *query)).WriteEndElement();
}

void BackupUserDataExportStat(DB::IDatabase& db, Util::XmlWriter& xmlWriter)
{
	static constexpr auto text = "select f.FolderTitle, b.FileName, u.ExportType, u.CreatedAt "
								 "from Export_List_User u "
								 "join Books b on b.BookID = u.BookID "
								 "join Folders f on f.FolderID = b.FolderID ";

	static constexpr const char* fields[] = {
		Constant::UserData::Books::Folder,
		Constant::UserData::Books::FileName,
		Constant::UserData::ExportStat::ExportType,
		Constant::UserData::Books::CreatedAt,
	};

	const auto query = db.CreateQuery(text);
	for (query->Execute(), assert(query->ColumnCount() == std::size(fields)); !query->Eof(); query->Next())
		xmlWriter.WriteStartElement(Constant::ITEM, XmlAttributes(fields, *query)).WriteEndElement();
}

void BackupUserDataFilter(DB::IDatabase& db, Util::XmlWriter& xmlWriter)
{
	auto range = IFilterProvider::GetDescriptions();
	(void)std::ranges::for_each(range,
	                            [&](const IFilterProvider::FilteredNavigation& description)
	                            {
									const auto index = static_cast<size_t>(description.navigationMode);
									assert(!Constant::UserData::Filter::FIELD_NAMES[index].empty());
									const auto query = db.CreateQuery(std::format("select {}, Flags from {} where Flags != 0", Constant::UserData::Filter::FIELD_NAMES[index], description.table));
									query->Execute();
									if (query->Eof())
										return;

									const auto navigationTitleGuard = xmlWriter.Guard(description.navigationTitle);
									for (; !query->Eof(); query->Next())
										navigationTitleGuard->WriteStartElement(Constant::ITEM)
											.WriteAttribute(Constant::UserData::Filter::Title, query->Get<const char*>(0))
											.WriteAttribute(Constant::UserData::Filter::Flag, query->Get<const char*>(1))
											.WriteEndElement();
								});
}

constexpr std::pair<const char*, BackupFunction> BACKUPERS[] {
#define ITEM(NAME) { Constant::UserData::NAME::RootNode, &BackupUserData##NAME }
	ITEM(Books), ITEM(Groups), ITEM(Searches), ITEM(ExportStat), ITEM(Filter),
#undef ITEM
};

} // namespace

void Backup(const Util::IExecutor& executor, DB::IDatabase& db, QString fileName, Callback callback)
{
	executor({ "Backup user data",
	           [&db, fileName = std::move(fileName), callback = std::move(callback)]() mutable
	           {
				   std::function<void(size_t)> result;

				   QFile out(fileName);
				   if (!out.open(QIODevice::WriteOnly))
				   {
					   auto error = QString(CANNOT_WRITE).arg(fileName);
					   PLOGE << error;
					   result = [error = std::move(error), callback = std::move(callback)](size_t) { callback(error); };
					   return result;
				   }

				   Util::XmlWriter xmlWriter(out);
				   ScopedCall rootElement([&] { xmlWriter.WriteStartElement(Constant::FlibraryBackup, XmlAttributes {}); }, [&] { xmlWriter.WriteEndElement(); });
				   ScopedCall(
					   [&]
					   {
						   xmlWriter.WriteStartElement(Constant::FlibraryBackupVersion,
			                                           XmlAttributes({
														   { Constant::VALUE, QString::number(Constant::FlibraryBackupVersionNumber) },
                           }));
					   },
					   [&] { xmlWriter.WriteEndElement(); });
				   ScopedCall userData([&] { xmlWriter.WriteStartElement(Constant::FlibraryUserData, XmlAttributes {}); }, [&] { xmlWriter.WriteEndElement(); });
				   for (const auto& [name, functor] : BACKUPERS)
				   {
					   ScopedCall item([&] { xmlWriter.WriteStartElement(name, XmlAttributes {}); }, [&] { xmlWriter.WriteEndElement(); });
					   functor(db, xmlWriter);
				   }

				   return result = [callback = std::move(callback)](size_t) { callback({}); };
			   } });
}

} // namespace HomeCompa::Flibrary::UserData
