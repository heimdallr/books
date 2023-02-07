#include <QXmlStreamReader>

#include "database/interface/Command.h"
#include "database/interface/Database.h"
#include "database/interface/Transaction.h"

#include "constants/ProductConstant.h"
#include "constants/UserData/books.h"
#include "constants/UserData/groups.h"
#include "constants/UserData/UserData.h"
#include "database/interface/Query.h"

namespace HomeCompa::Flibrary::UserData {

void RestoreBooks1(class DB::Database & db, class QXmlStreamReader & reader)
{
	using namespace Constant::UserData::Books;
	static constexpr auto commandText =
		"insert into Books_User(BookID, IsDeleted) "
		"select BookID, ? "
		"from Books "
		"where Folder = ? and FileName = ?"
		;
	assert(reader.name() == RootNode);

	const auto transaction = db.CreateTransaction();
	transaction->CreateCommand("delete from Books_User")->Execute();

	const auto command = transaction->CreateCommand(commandText);

	while (!reader.atEnd() && !reader.hasError())
	{
		const auto token = reader.readNext();
		if (token == QXmlStreamReader::EndElement && reader.name() == RootNode)
			return transaction->Commit();

		if (token != QXmlStreamReader::StartElement)
			continue;

		assert(reader.name() == Flibrary::Constant::ITEM);

		const auto attributes = reader.attributes();
		assert(true
			&& attributes.hasAttribute(Folder)
			&& attributes.hasAttribute(FileName)
			&& attributes.hasAttribute(IsDeleted)
		);
		command->Bind(0, attributes.value(IsDeleted).toString().toInt());
		command->Bind(1, attributes.value(Folder).toString().toStdString());
		command->Bind(2, attributes.value(FileName).toString().toStdString());

		command->Execute();
	}
}

void RestoreGroups1(class DB::Database & db, class QXmlStreamReader & reader)
{
	using namespace Constant::UserData;
	assert(reader.name() == Groups::RootNode);

	static constexpr auto addBookToGroupCommandText =
		"insert into Groups_List_User(GroupID, BookID) "
		"select ?, BookID "
		"from Books "
		"where Folder = ? and FileName = ?"
		;

	const auto transaction = db.CreateTransaction();
	transaction->CreateCommand("delete from Groups_User")->Execute();

	const auto createGroupCommand = transaction->CreateCommand(Groups::CreateNewGroupCommandText);
	const auto addBookToGroupCommand = transaction->CreateCommand(addBookToGroupCommandText);

	long long groupId = -1;

	while (!reader.atEnd() && !reader.hasError())
	{
		const auto token = reader.readNext();
		if (token == QXmlStreamReader::EndElement && reader.name() == Groups::RootNode)
			return transaction->Commit();

		if (token != QXmlStreamReader::StartElement)
			continue;

		const auto mode = reader.name();
		const auto attributes = reader.attributes();

		if (mode == Groups::GroupNode)
		{
			assert(attributes.hasAttribute(Constant::TITLE));
			const auto title = attributes.value(Constant::TITLE).toString().toStdString();
			createGroupCommand->Bind(0, title);
			createGroupCommand->Execute();
			const auto getLastIdQuery = transaction->CreateQuery(SelectLastIdQueryText);
			getLastIdQuery->Execute();
			groupId = getLastIdQuery->Get<long long>(0);
			continue;
		}

		if (mode == Constant::ITEM)
		{
			assert(groupId >= 0);
			assert(attributes.hasAttribute(Books::Folder) && attributes.hasAttribute(Books::FileName));
			addBookToGroupCommand->Bind(0, groupId);
			addBookToGroupCommand->Bind(1, attributes.value(Books::Folder).toString().toStdString());
			addBookToGroupCommand->Bind(2, attributes.value(Books::FileName).toString().toStdString());

			addBookToGroupCommand->Execute();
		}
	}
}

}
