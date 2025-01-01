#include <QStringList>

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

	explicit Book(const Util::XmlAttributes & attributes)
		: folder(attributes.GetAttribute(Constant::UserData::Books::Folder))
		, fileName(attributes.GetAttribute(Constant::UserData::Books::FileName))
	{
	}
};
using Books = std::vector<Book>;

class BooksRestorer final
	: virtual public IRestorer
{
	struct Item : Book
	{
		int isDeleted { 0 };

		explicit Item(const Util::XmlAttributes & attributes)
			: Book(attributes)
			, isDeleted(attributes.GetAttribute(Constant::UserData::Books::IsDeleted).toInt())
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
			"insert into Books_User(BookID, IsDeleted) "
			"select BookID, ? "
			"from Books "
			"where Folder = ? and FileName = ?"
			;

		const auto transaction = db.CreateTransaction();
		transaction->CreateCommand("delete from Books_User")->Execute();

		const auto command = transaction->CreateCommand(commandText);
		for (const auto & item : m_items)
		{
			command->Bind(0, item.isDeleted);
			command->Bind(1, item.folder.toStdString());
			command->Bind(2, item.fileName.toStdString());
			command->Execute();
		}

		transaction->Commit();
	}

private:
	Items m_items;
};

class GroupsRestorer final
	: virtual public IRestorer
{
	struct Item
	{
		QString title;
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
			"insert into Groups_List_User(GroupID, BookID) "
			"select ?, BookID "
			"from Books "
			"where Folder = ? and FileName = ?"
			;

		const auto transaction = db.CreateTransaction();
		transaction->CreateCommand("delete from Groups_User")->Execute();

		const auto createGroupCommand = transaction->CreateCommand(Constant::UserData::Groups::CreateNewGroupCommandText);
		const auto addBookToGroupCommand = transaction->CreateCommand(addBookToGroupCommandText);

		for (const auto & [group, books] : m_items)
		{
			createGroupCommand->Bind(0, group.toStdString());
			createGroupCommand->Execute();

			const auto getLastIdQuery = transaction->CreateQuery(IDatabaseUser::SELECT_LAST_ID_QUERY);
			getLastIdQuery->Execute();
			const auto groupId = getLastIdQuery->Get<long long>(0);

			for (const auto & [folder, fileName] : books)
			{
				addBookToGroupCommand->Bind(0, groupId);
				addBookToGroupCommand->Bind(1, folder.toStdString());
				addBookToGroupCommand->Bind(2, fileName.toStdString());
				addBookToGroupCommand->Execute();
			}
		}

		transaction->Commit();
	}

private:
	void AddGroup(const Util::XmlAttributes & attributes)
	{
		m_items.emplace_back(attributes.GetAttribute(Constant::TITLE), Books {});
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
		m_items << attributes.GetAttribute(Constant::TITLE);
	}

	void Restore(DB::IDatabase & db) const override
	{
		using namespace Constant::UserData;
		const auto transaction = db.CreateTransaction();
		transaction->CreateCommand("delete from Searches_User")->Execute();
		const auto createSearchCommand = transaction->CreateCommand(Searches::CreateNewSearchCommandText);
		for (const auto & item : m_items)
		{
			createSearchCommand->Bind(0, item.toStdString());
			createSearchCommand->Execute();
		}

		transaction->Commit();
	}

private:
	QStringList m_items;
};

}

}

namespace HomeCompa::Flibrary::UserData {

std::unique_ptr<IRestorer> CreateBooksRestorer1()
{
	return std::make_unique<BooksRestorer>();
}
std::unique_ptr<IRestorer> CreateGroupsRestorer1()
{
	return std::make_unique<GroupsRestorer>();
}
std::unique_ptr<IRestorer> CreateSearchesRestorer1()
{
	return std::make_unique<SearchesRestorer>();
}

}
