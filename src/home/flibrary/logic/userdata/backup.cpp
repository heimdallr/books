#include "backup.h"

#include <QFile>
#include <QString>

#include <plog/Log.h>

#include "fnd/FindPair.h"
#include "fnd/ScopedCall.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "util/IExecutor.h"

#include "interface/constants/Localization.h"
#include "interface/constants/ProductConstant.h"

#include "constants/books.h"
#include "constants/ExportStat.h"
#include "constants/groups.h"
#include "constants/searches.h"

#include "util/xml/XmlAttributes.h"
#include "util/xml/XmlWriter.h"

namespace HomeCompa::Flibrary::UserData {

namespace {

constexpr auto CANNOT_WRITE = QT_TRANSLATE_NOOP("UserData", "Cannot write to '%1'");

using BackupFunction = void(*)(DB::IDatabase & db, Util::XmlWriter &);

class XmlAttributes final
	: public Util::XmlAttributes
{
public:
	template <std::ranges::input_range R>
	XmlAttributes(const R & r, DB::IQuery & query)
	{
		m_values.reserve(std::size(r));
		std::ranges::transform(r, std::back_inserter(m_values), [&, n = 0] (const auto & value) mutable
		{
			return std::pair(value, query.Get<const char *>(n++));
		});
	}

	XmlAttributes() = default;

	XmlAttributes(std::vector<std::pair<QString, QString>> values)
		: m_values(std::move(values))
	{
	}

private: // Util::XmlAttributes
	QString GetAttribute(const QString & key) const override
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

void BackupUserDataBooks(DB::IDatabase & db, Util::XmlWriter & xmlWriter)
{
	static constexpr auto text =
		"select b.Folder, b.FileName, u.IsDeleted, u.UserRate, u.CreatedAt "
		"from Books_User u "
		"join Books b on b.BookID = u.BookID"
		;

	static constexpr const char * fields[] =
	{
		Constant::UserData::Books::Folder,
		Constant::UserData::Books::FileName,
		Constant::UserData::Books::IsDeleted,
		Constant::UserData::Books::UserRate,
		Constant::UserData::Books::CreatedAt,
	};

	const auto query = db.CreateQuery(text);
	for (query->Execute(), assert(query->ColumnCount() == std::size(fields)); !query->Eof(); query->Next())
		xmlWriter.WriteStartElement(Constant::ITEM, XmlAttributes(fields, *query)).WriteEndElement();
}

void BackupUserDataGroups(DB::IDatabase & db, Util::XmlWriter & xmlWriter)
{
	static constexpr auto text =
		"select b.Folder, b.FileName, gl.CreatedAt, g.Title, g.CreatedAt groupCreatedAt "
		"from Groups_User g "
		"join Groups_List_User gl on gl.GroupID = g.GroupID "
		"join Books b on b.BookID = gl.BookID "
		"order by g.GroupID"
		;

	static constexpr const char * fields[] =
	{
		Constant::UserData::Books::Folder,
		Constant::UserData::Books::FileName,
		Constant::UserData::Books::CreatedAt,
	};

	std::unique_ptr<ScopedCall> group;
	QString currentTitle;
	const auto query = db.CreateQuery(text);
	for (query->Execute(); !query->Eof(); query->Next())
	{
		QString title = query->Get<const char *>(3);
		QString groupCreatedAt = query->Get<const char *>(4);
		if (currentTitle != title)
		{
			currentTitle = std::move(title);
			group.reset();
			std::make_unique<ScopedCall>([&]
			{
				xmlWriter.WriteStartElement(Constant::UserData::Groups::GroupNode, XmlAttributes({
					{ Constant::TITLE, currentTitle },
					{ Constant::UserData::Books::CreatedAt, groupCreatedAt},
				}));
			}, [&]
			{
				xmlWriter.WriteEndElement();
			}).swap(group);
		}

		xmlWriter.WriteStartElement(Constant::ITEM, XmlAttributes(fields, *query)).WriteEndElement();
	}
}

void BackupUserDataSearches(DB::IDatabase & db, Util::XmlWriter & xmlWriter)
{
	static constexpr auto text = "select s.Title, s.CreatedAt from Searches_User s ";

	static constexpr const char * fields[] =
	{
		Constant::TITLE,
		Constant::UserData::Books::CreatedAt,
	};

	const auto query = db.CreateQuery(text);
	for (query->Execute(); !query->Eof(); query->Next())
		xmlWriter.WriteStartElement(Constant::ITEM, XmlAttributes(fields, *query)).WriteEndElement();
}

void BackupUserDataExportStat(DB::IDatabase & db, Util::XmlWriter & xmlWriter)
{
	static constexpr auto text =
		"select b.Folder, b.FileName, u.ExportType, u.CreatedAt "
		"from Export_List_User u "
		"join Books b on b.BookID = u.BookID"
		;

	static constexpr const char * fields[] =
	{
		Constant::UserData::Books::Folder,
		Constant::UserData::Books::FileName,
		Constant::UserData::ExportStat::ExportType,
		Constant::UserData::Books::CreatedAt,
	};

	const auto query = db.CreateQuery(text);
	for (query->Execute(), assert(query->ColumnCount() == std::size(fields)); !query->Eof(); query->Next())
		xmlWriter.WriteStartElement(Constant::ITEM, XmlAttributes(fields, *query)).WriteEndElement();
}

constexpr std::pair<const char *, BackupFunction> BACKUPERS[]
{
#define ITEM(NAME) { Constant::UserData::NAME::RootNode, &BackupUserData##NAME }
		ITEM(Books),
		ITEM(Groups),
		ITEM(Searches),
		ITEM(ExportStat),
#undef	ITEM
};

}

void Backup(const Util::IExecutor & executor, DB::IDatabase & db, QString fileName, Callback callback)
{
	executor({ "Backup user data", [&db, fileName = std::move(fileName), callback = std::move(callback)] () mutable
	{
		std::function<void(size_t)> result;

		QFile out(fileName);
		if (!out.open(QIODevice::WriteOnly))
		{
			auto error = QString(CANNOT_WRITE).arg(fileName);
			PLOGE << error;
			result = [error = std::move(error), callback = std::move(callback)] (size_t) { callback(error); };
			return result;
		}

		Util::XmlWriter xmlWriter(out);
		ScopedCall rootElement([&] { xmlWriter.WriteStartElement(Constant::FlibraryBackup, XmlAttributes{}); }, [&] { xmlWriter.WriteEndElement(); });
		ScopedCall([&]
		{
			xmlWriter.WriteStartElement(Constant::FlibraryBackupVersion, XmlAttributes({ { Constant::VALUE, QString::number(Constant::FlibraryBackupVersionNumber) }, }));
		}, [&]
			{
				xmlWriter.WriteEndElement();
			});
		ScopedCall userData([&] { xmlWriter.WriteStartElement(Constant::FlibraryUserData, XmlAttributes {}); }, [&] { xmlWriter.WriteEndElement(); });
		for (const auto & [name, functor] : BACKUPERS)
		{
			ScopedCall item([&] { xmlWriter.WriteStartElement(name, XmlAttributes{}); }, [&] { xmlWriter.WriteEndElement(); });
			functor(db, xmlWriter);
		}

		return result = [callback = std::move(callback)](size_t){ callback({}); };
	} });
}

}
