#include <QString>

#include "fnd/FindPair.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/ProductConstant.h"
#include "interface/logic/IDatabaseUser.h"

#include "constants/books.h"
#include "constants/groups.h"
#include "constants/searches.h"

#include "util/xml/XmlAttributes.h"

#include "restore.h"

namespace HomeCompa::Flibrary::UserData {

namespace {

struct Book
{
	QString folder;
	QString fileName;
	QString createdAt;

	explicit Book(const Util::XmlAttributes & attributes)
		: folder(attributes.GetAttribute(Constant::UserData::Books::Folder))
		, fileName(attributes.GetAttribute(Constant::UserData::Books::FileName))
		, createdAt(attributes.GetAttribute(Constant::UserData::Books::CreatedAt))
	{
	}
};
using Books = std::vector<Book>;

#define ADDITIONAL_BOOK_FIELDS_X_MACRO    \
		ADDITIONAL_BOOK_FIELD(IsDeleted)  \
		ADDITIONAL_BOOK_FIELD(UserRate)   \
		ADDITIONAL_BOOK_FIELD(CreatedAt)

struct FieldNo
{
	enum
	{
#define ADDITIONAL_BOOK_FIELD(NAME) NAME,
		ADDITIONAL_BOOK_FIELDS_X_MACRO
#undef	ADDITIONAL_BOOK_FIELD
		Folder,
		FileName,
	};
};

struct Created
{
	QString title;
	QString createdAt;
};

class BooksRestorer final
	: virtual public IRestorer
{
	struct Item : Book
	{
#define ADDITIONAL_BOOK_FIELD(NAME) QString NAME;
		ADDITIONAL_BOOK_FIELDS_X_MACRO
#undef	ADDITIONAL_BOOK_FIELD

		explicit Item(const Util::XmlAttributes & attributes)
			: Book(attributes)
#define	ADDITIONAL_BOOK_FIELD(NAME) , NAME(attributes.GetAttribute(Constant::UserData::Books::NAME))
		ADDITIONAL_BOOK_FIELDS_X_MACRO
#undef	ADDITIONAL_BOOK_FIELD
		{
		}
	};
	using Items = std::vector<Item>;

private: // IRestorer
	void AddElement([[maybe_unused]] const QString & name, const Util::XmlAttributes & attributes) override
	{
		assert(name == Constant::ITEM);
		using namespace Constant::UserData::Books;
		m_items.emplace_back(attributes);
	}

	void Restore(DB::IDatabase & db) const override
	{
		using namespace Constant::UserData::Books;
		static constexpr auto commandText =
			"insert into Books_User(BookID, IsDeleted, UserRate, CreatedAt) "
			"select BookID, ?, ?, ? "
			"from Books "
			"where Folder = ? and FileName = ?"
			;

		const auto transaction = db.CreateTransaction();
		transaction->CreateCommand("delete from Books_User")->Execute();

		const auto command = transaction->CreateCommand(commandText);

		const auto bind = [&] (const size_t index, const QString & value)
		{
			if (value.isEmpty())
				return (void)command->Bind(index);

			bool ok = false;
			if (const auto integer = value.toInt(&ok); ok)
				command->Bind(index, integer);
			else
				command->Bind(index, value.toStdString());
		};

		for (const auto & item : m_items)
		{
#define ADDITIONAL_BOOK_FIELD(NAME) bind(FieldNo::NAME, item.NAME);
			ADDITIONAL_BOOK_FIELDS_X_MACRO
#undef	ADDITIONAL_BOOK_FIELD

			command->Bind(FieldNo::Folder, item.folder.toStdString());
			command->Bind(FieldNo::FileName, item.fileName.toStdString());
			command->Execute();
		}

		transaction->Commit();
	}

private:
	Items m_items;
};

#undef ADDITIONAL_BOOK_FIELDS_X_MACRO

void Bind(DB::ICommand & command, const size_t index, const QString & value)
{
	if (value.isEmpty())
		command.Bind(index);
	else
		command.Bind(index, value.toStdString());
};


class GroupsRestorer final
	: virtual public IRestorer
{
	struct Item : Created
	{
		Books books;
	};
	using Items = std::vector<Item>;

private: // IRestorer
	void AddElement(const QString & name, const Util::XmlAttributes & attributes) override
	{
		using Functor = void(GroupsRestorer::*)(const Util::XmlAttributes &);
		constexpr std::pair<const char *, Functor> collectors[]
		{
#define		ITEM(NAME) {#NAME, &GroupsRestorer::Add##NAME}
			ITEM(Group),
			ITEM(Item),
#undef		ITEM
		};

		const auto invoker = FindSecond(collectors, name.toStdString().data(), PszComparer{});
		std::invoke(invoker, this, std::cref(attributes));
	}

	void Restore(DB::IDatabase & db) const override
	{
		static constexpr auto addBookToGroupCommandText =
			"insert into Groups_List_User(GroupID, BookID, CreatedAt) "
			"select ?, BookID, ? "
			"from Books "
			"where Folder = ? and FileName = ?"
			;

		const auto transaction = db.CreateTransaction();
		transaction->CreateCommand("delete from Groups_User")->Execute();

		const auto createGroupCommand = transaction->CreateCommand(Constant::UserData::Groups::CreateNewGroupCommandText);
		const auto addBookToGroupCommand = transaction->CreateCommand(addBookToGroupCommandText);

		const auto addBooksToGroup = [&] (const Books& books)
		{
			const auto getLastIdQuery = transaction->CreateQuery(IDatabaseUser::SELECT_LAST_ID_QUERY);
			getLastIdQuery->Execute();
			const auto groupId = getLastIdQuery->Get<long long>(0);

			for (const auto & [folder, fileName, createdAt] : books)
			{
				addBookToGroupCommand->Bind(0, groupId);
				Bind(*addBookToGroupCommand, 1, createdAt);
				addBookToGroupCommand->Bind(2, folder.toStdString());
				addBookToGroupCommand->Bind(3, fileName.toStdString());
				addBookToGroupCommand->Execute();
			}
		};

		for (const auto & item : m_items)
		{
			createGroupCommand->Bind(0, item.title.toStdString());
			Bind(*createGroupCommand, 1, item.createdAt);
			createGroupCommand->Execute();
			addBooksToGroup(item.books);
		}

		transaction->Commit();
	}

private:
	void AddGroup(const Util::XmlAttributes & attributes)
	{
		auto & item = m_items.emplace_back();
		item.title = attributes.GetAttribute(Constant::TITLE);
		item.createdAt = attributes.GetAttribute(Constant::UserData::Books::CreatedAt);
	}

	void AddItem(const Util::XmlAttributes & attributes)
	{
		assert(!m_items.empty());
		m_items.back().books.emplace_back(attributes);
	}

private:
	Items m_items;
};

class SearchesRestorer final
	: virtual public IRestorer
{
private: // IRestorer
	void AddElement([[maybe_unused]] const QString & name, const Util::XmlAttributes & attributes) override
	{
		assert(name == Constant::ITEM);
		auto & item = m_items.emplace_back();
		item.title = attributes.GetAttribute(Constant::TITLE);
		item.createdAt = attributes.GetAttribute(Constant::UserData::Books::CreatedAt);
	}

	void Restore(DB::IDatabase & db) const override
	{
		using namespace Constant::UserData;
		const auto transaction = db.CreateTransaction();
		transaction->CreateCommand("delete from Searches_User")->Execute();
		const auto createSearchCommand = transaction->CreateCommand(Searches::CreateNewSearchCommandText);
		for (const auto & [title, createdAt] : m_items)
		{
			createSearchCommand->Bind(0, title.toStdString());
			Bind(*createSearchCommand, 1, createdAt);
			createSearchCommand->Execute();
		}

		transaction->Commit();
	}

private:
	std::vector<Created> m_items;
};

}

}

namespace HomeCompa::Flibrary::UserData {

std::unique_ptr<IRestorer> CreateBooksRestorer3()
{
	return std::make_unique<BooksRestorer>();
}
std::unique_ptr<IRestorer> CreateGroupsRestorer3()
{
	return std::make_unique<GroupsRestorer>();
}
std::unique_ptr<IRestorer> CreateSearchesRestorer3()
{
	return std::make_unique<SearchesRestorer>();
}

}
