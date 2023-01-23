#include <QFile>
#include <QString>
#include <QXmlStreamWriter>

#include "fnd/ScopedCall.h"

#include "database/interface/Database.h"
#include "database/interface/Query.h"

#include "util/executor.h"

#include "constants/ProductConstant.h"

#include "backup.h"

namespace HomeCompa::Flibrary {

namespace {

using BackupFunction = void(*)(DB::Database & db, QXmlStreamWriter & stream);

void BackupUserDataBooks(DB::Database & db, QXmlStreamWriter & stream)
{
	static constexpr auto text =
		"select b.Folder, b.FileName, u.IsDeleted "
		"from Books_User u "
		"join Books b on b.BookID = u.BookID"
		;

	static constexpr const char * fields[] =
	{
		"Folder",
		"FileName",
		"IsDeleted",
	};

	const auto query = db.CreateQuery(text);
	for (query->Execute(), assert(query->ColumnCount() == std::size(fields)); !query->Eof(); query->Next())
	{
		ScopedCall item([&] { stream.writeStartElement(Constant::ITEM); }, [&] { stream.writeEndElement(); });
		for (size_t i = 0, sz = std::size(fields); i < sz; ++i)
			assert(query->ColumnName(i) == fields[i]), stream.writeAttribute(fields[i], query->Get<const char *>(i));
	}
}

void BackupUserDataGroups(DB::Database & db, QXmlStreamWriter & stream)
{
	static constexpr auto text =
		"select g.Title, b.Folder, b.FileName "
		"from Groups_User g "
		"join Groups_List_User gl on gl.GroupID = g.GroupID "
		"join Books b on b.BookID = gl.BookID "
		"order by g.GroupID"
		;

	std::unique_ptr<ScopedCall> group;
	std::string currentTitle;
	const auto query = db.CreateQuery(text);
	for (query->Execute(); !query->Eof(); query->Next())
	{
		const auto * title = query->Get<const char *>(0);
		if (currentTitle != title)
		{
			currentTitle = title;
			group.reset();
			std::make_unique<ScopedCall>([&] { stream.writeStartElement("Group"); }, [&] { stream.writeEndElement(); }).swap(group);
			stream.writeAttribute(Constant::TITLE, currentTitle.data());
		}

		ScopedCall item([&] { stream.writeStartElement(Constant::ITEM); }, [&] { stream.writeEndElement(); });
		stream.writeAttribute("Folder", query->Get<const char *>(1));
		stream.writeAttribute("FileName", query->Get<const char *>(2));
	}
}

constexpr std::pair<const char *, BackupFunction> g_backupers[]
{
#define ITEM(NAME) { #NAME, &BackupUserData##NAME }
		ITEM(Books),
		ITEM(Groups),
#undef	ITEM
};

}

void Backup(Util::Executor & executor, DB::Database & db, const QString & fileName)
{
	executor({ "Backup user data", [&db, fileName]
	{
		QFile outp(fileName);
		outp.open(QIODevice::WriteOnly);

		QXmlStreamWriter stream(&outp);
		stream.setAutoFormatting(true);
		ScopedCall document([&] { stream.writeStartDocument(); }, [&] { stream.writeEndDocument(); });
		ScopedCall rootElement([&] { stream.writeStartElement(Constant::FlibraryBackup); }, [&] { stream.writeEndElement(); });
		ScopedCall ([&] { stream.writeStartElement(Constant::FlibraryBackupVersion); stream.writeAttribute(Constant::VALUE, QString::number(Constant::FlibraryBackupVersionNumber)); }, [&] { stream.writeEndElement(); });
		ScopedCall userData([&] { stream.writeStartElement(Constant::FlibraryUserData); }, [&] { stream.writeEndElement(); });
		for (const auto & [name, functor] : g_backupers)
		{
			ScopedCall item([&] { stream.writeStartElement(name); }, [&] { stream.writeEndElement(); });
			functor(db, stream);
		}

		return [] {};
	} });
}

}
