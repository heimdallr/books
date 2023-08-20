#include <QXmlStreamReader>

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/ProductConstant.h"
#include "constants/books.h"
#include "constants/groups.h"
#include "constants/UserData.h"
#include "database/interface/IQuery.h"
#include "shared/DatabaseUser.h"

namespace HomeCompa::Flibrary::UserData {

void RestoreBooks1(DB::IDatabase & db, QXmlStreamReader & reader)
{
	using namespace Constant::UserData::Books;
	static constexpr auto commandText =
		"insert into Books_User(BookID, IsDeleted) "
		"select BookID, ? "
		"from Books "
		"where Folder = ? and FileName = ?"
		;
	assert(reader.name().compare(RootNode) == 0);

	const auto transaction = db.CreateTransaction();
	transaction->CreateCommand("delete from Books_User")->Execute();

	const auto command = transaction->CreateCommand(commandText);

	while (!reader.atEnd() && !reader.hasError())
	{
		const auto token = reader.readNext();
		if (token == QXmlStreamReader::EndElement && reader.name().compare(RootNode) == 0)
			return transaction->Commit();

		if (token != QXmlStreamReader::StartElement)
			continue;

		assert(reader.name().compare(Flibrary::Constant::ITEM) == 0);

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

void RestoreGroups1(DB::IDatabase & db, QXmlStreamReader & reader)
{
	using namespace Constant::UserData;
	assert(reader.name().compare(Groups::RootNode) == 0);

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
		if (token == QXmlStreamReader::EndElement && reader.name().compare(Groups::RootNode) == 0)
			return transaction->Commit();

		if (token != QXmlStreamReader::StartElement)
			continue;

		const auto mode = reader.name();
		const auto attributes = reader.attributes();

		if (mode.compare(Groups::GroupNode) == 0)
		{
			assert(attributes.hasAttribute(Constant::TITLE));
			const auto title = attributes.value(Constant::TITLE).toString().toStdString();
			createGroupCommand->Bind(0, title);
			createGroupCommand->Execute();
			const auto getLastIdQuery = transaction->CreateQuery(DatabaseUser::SELECT_LAST_ID_QUERY);
			getLastIdQuery->Execute();
			groupId = getLastIdQuery->Get<long long>(0);
			continue;
		}

		if (mode.compare(Constant::ITEM) == 0)
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
