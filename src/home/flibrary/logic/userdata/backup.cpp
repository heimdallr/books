#include <QFile>
#include <QString>
#include <QXmlStreamWriter>

#include <plog/Log.h>

#include "fnd/ScopedCall.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "util/IExecutor.h"

#include "interface/constants/Localization.h"
#include "interface/constants/ProductConstant.h"

#include "constants/books.h"
#include "constants/groups.h"

#include "backup.h"

namespace HomeCompa::Flibrary::UserData {

namespace {

constexpr auto CANNOT_WRITE = QT_TRANSLATE_NOOP("UserData", "Cannot write to '%1'");

using BackupFunction = void(*)(DB::IDatabase & db, QXmlStreamWriter & stream);

void BackupUserDataBooks(DB::IDatabase & db, QXmlStreamWriter & stream)
{
	static constexpr auto text =
		"select b.Folder, b.FileName, u.IsDeleted "
		"from Books_User u "
		"join Books b on b.BookID = u.BookID"
		;

	static constexpr const char * fields[] =
	{
		Constant::UserData::Books::Folder,
		Constant::UserData::Books::FileName,
		Constant::UserData::Books::IsDeleted,
	};

	const auto query = db.CreateQuery(text);
	for (query->Execute(), assert(query->ColumnCount() == std::size(fields)); !query->Eof(); query->Next())
	{
		ScopedCall item([&] { stream.writeStartElement(Constant::ITEM); }, [&] { stream.writeEndElement(); });
		for (size_t i = 0, sz = std::size(fields); i < sz; ++i)
			assert(query->ColumnName(i) == fields[i]), stream.writeAttribute(fields[i], query->Get<const char *>(i));
	}
}

void BackupUserDataGroups(DB::IDatabase & db, QXmlStreamWriter & stream)
{
	static constexpr auto text =
		"select g.Title, b.Folder, b.FileName "
		"from Groups_User g "
		"join Groups_List_User gl on gl.GroupID = g.GroupID "
		"join Books b on b.BookID = gl.BookID "
		"order by g.GroupID"
		;

	std::unique_ptr<ScopedCall> group;
	QString currentTitle;
	const auto query = db.CreateQuery(text);
	for (query->Execute(); !query->Eof(); query->Next())
	{
		QString title = query->Get<const char *>(0);
		if (currentTitle != title)
		{
			currentTitle = title;
			group.reset();
			std::make_unique<ScopedCall>([&] { stream.writeStartElement(Constant::UserData::Groups::GroupNode); }, [&] { stream.writeEndElement(); }).swap(group);
			stream.writeAttribute(Constant::TITLE, currentTitle);
		}

		ScopedCall item([&] { stream.writeStartElement(Constant::ITEM); }, [&] { stream.writeEndElement(); });
		stream.writeAttribute(Constant::UserData::Books::Folder, query->Get<const char *>(1));
		stream.writeAttribute(Constant::UserData::Books::FileName, query->Get<const char *>(2));
	}
}

constexpr std::pair<const char *, BackupFunction> BACKUPERS[]
{
#define ITEM(NAME) { #NAME, &BackupUserData##NAME }
		ITEM(Books),
		ITEM(Groups),
#undef	ITEM
};

}

void Backup(Util::IExecutor & executor, DB::IDatabase & db, QString fileName, Callback callback)
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

		QXmlStreamWriter stream(&out);
		stream.setAutoFormatting(true);
		ScopedCall document([&] { stream.writeStartDocument(); }, [&] { stream.writeEndDocument(); });
		ScopedCall rootElement([&] { stream.writeStartElement(Constant::FlibraryBackup); }, [&] { stream.writeEndElement(); });
		ScopedCall ([&] { stream.writeStartElement(Constant::FlibraryBackupVersion); stream.writeAttribute(Constant::VALUE, QString::number(Constant::FlibraryBackupVersionNumber)); }, [&] { stream.writeEndElement(); });
		ScopedCall userData([&] { stream.writeStartElement(Constant::FlibraryUserData); }, [&] { stream.writeEndElement(); });
		for (const auto & [name, functor] : BACKUPERS)
		{
			ScopedCall item([&] { stream.writeStartElement(name); }, [&] { stream.writeEndElement(); });
			functor(db, stream);
		}

		return result = [callback = std::move(callback)](size_t){ callback({}); };
	} });
}

}
