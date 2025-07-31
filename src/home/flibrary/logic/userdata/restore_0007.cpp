#include "fnd/FindPair.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/ProductConstant.h"
#include "interface/logic/IDatabaseUser.h"

#include "constants/books.h"
#include "constants/groups.h"
#include "util/xml/XmlAttributes.h"

#include "restore.h"

namespace HomeCompa::Flibrary::UserData
{

namespace
{

void BindImpl(DB::ICommand& command, const size_t index, const QString& value)
{
	if (value.isEmpty())
		command.Bind(index);
	else
		command.Bind(index, value.toStdString());
}

class GroupsRestorer final : virtual public IRestorer
{
	struct Item
	{
		QString title;
		QString createdAt;

		explicit Item(const Util::XmlAttributes& attributes)
			: title { attributes.GetAttribute(Constant::TITLE) }
			, createdAt { attributes.GetAttribute(Constant::UserData::Books::CreatedAt) }
		{
		}

		void Write(DB::ICommand& command, const long long groupId) const
		{
			command.Bind(0, groupId);
			BindImpl(command, 1, createdAt);
			BindImpl(command, 2, title);
			command.Execute();
		}
	};

	using Items = std::vector<Item>;

	struct Book
	{
		QString file;
		QString folder;
		QString createdAt;

		explicit Book(const Util::XmlAttributes& attributes)
			: file { attributes.GetAttribute(Constant::UserData::Books::FileName) }
			, folder { attributes.GetAttribute(Constant::UserData::Books::Folder) }
			, createdAt { attributes.GetAttribute(Constant::UserData::Books::CreatedAt) }
		{
		}

		void Write(DB::ICommand& command, const long long groupId) const
		{
			command.Bind(0, groupId);
			BindImpl(command, 1, createdAt);
			BindImpl(command, 2, folder);
			BindImpl(command, 3, file);
			command.Execute();
		}
	};

	using Books = std::vector<Book>;

	struct Group : Item
	{
		Books books;
		Items authors, series, keywords;

		explicit Group(const Util::XmlAttributes& attributes)
			: Item(attributes)
		{
		}

		void Write(DB::ICommand& command) const
		{
			BindImpl(command, 0, title);
			BindImpl(command, 1, createdAt);
			command.Execute();
		}
	};

	using Groups = std::vector<Group>;
	using GroupRef = std::reference_wrapper<const Group>;

private: // IRestorer
	void AddElement(const QString& name, const Util::XmlAttributes& attributes) override
	{
		using Functor = void (GroupsRestorer::*)(const Util::XmlAttributes&);
		constexpr std::pair<const char*, Functor> collectors[] {
#define ITEM(NAME) { #NAME, &GroupsRestorer::Add##NAME }
			ITEM(Group), ITEM(Item), ITEM(Author), ITEM(Series), ITEM(Keyword),
#undef ITEM
		};

		const auto invoker = FindSecond(collectors, name.toStdString().data(), PszComparer {});
		std::invoke(invoker, this, std::cref(attributes));
	}

	void Restore(DB::IDatabase& db) const override
	{
		static constexpr auto commandTextBooks = "insert into Groups_List_User(GroupID, ObjectID, CreatedAt) "
												 "select ?, BookID, ? "
												 "from Books "
												 "where FolderID = (select FolderID from Folders where FolderTitle = ?) and FileName = ?";
		static constexpr auto commandTextAuthors = "insert into Groups_List_User(GroupID, ObjectID, CreatedAt) "
												   "select ?, AuthorID, ? "
												   "from Authors "
												   "where LastName||','||FirstName||','||MiddleName = ?";
		static constexpr auto commandTextSeries = "insert into Groups_List_User(GroupID, ObjectID, CreatedAt) "
												  "select ?, SeriesID, ? "
												  "from Series "
												  "where SeriesTitle = ?";
		static constexpr auto commandTextKeywords = "insert into Groups_List_User(GroupID, ObjectID, CreatedAt) "
													"select ?, KeywordID, ? "
													"from Keywords "
													"where KeywordTitle = ?";

		const auto transaction = db.CreateTransaction();
		transaction->CreateCommand("delete from Groups_User")->Execute();
		const auto groups = WriteGroups(*transaction);

		WriteItems<Books>(*transaction, commandTextBooks, groups, &Group::books);
		WriteItems<Items>(*transaction, commandTextAuthors, groups, &Group::authors);
		WriteItems<Items>(*transaction, commandTextSeries, groups, &Group::series);
		WriteItems<Items>(*transaction, commandTextKeywords, groups, &Group::keywords);

		transaction->Commit();
	}

private:
	std::vector<std::pair<GroupRef, long long>> WriteGroups(DB::ITransaction& tr) const
	{
		std::vector<std::pair<GroupRef, long long>> groups;
		const auto command = tr.CreateCommand(Constant::UserData::Groups::CreateNewGroupCommandText);
		std::ranges::transform(m_groups,
		                       std::back_inserter(groups),
		                       [&](const Group& group)
		                       {
								   group.Write(*command);
								   const auto getLastIdQuery = tr.CreateQuery(IDatabaseUser::SELECT_LAST_ID_QUERY);
								   getLastIdQuery->Execute();
								   return std::make_pair(GroupRef { group }, getLastIdQuery->Get<long long>(0));
							   });
		return groups;
	}

	template <std::ranges::range Container>
	void WriteItems(DB::ITransaction& tr, const char* commandText, const std::vector<std::pair<GroupRef, long long>>& groups, const std::function<const Container&(const Group&)>& functor) const
	{
		for (const auto command = tr.CreateCommand(commandText); const auto& [group, groupId] : groups)
			for (const auto& book : functor(group.get()))
				book.Write(*command, groupId);
	}

	void AddGroup(const Util::XmlAttributes& attributes)
	{
		m_groups.emplace_back(attributes);
	}

	void AddItem(const Util::XmlAttributes& attributes)
	{
		assert(!m_groups.empty());
		m_groups.back().books.emplace_back(attributes);
	}

	void AddAuthor(const Util::XmlAttributes& attributes)
	{
		assert(!m_groups.empty());
		m_groups.back().authors.emplace_back(attributes);
	}

	void AddSeries(const Util::XmlAttributes& attributes)
	{
		assert(!m_groups.empty());
		m_groups.back().series.emplace_back(attributes);
	}

	void AddKeyword(const Util::XmlAttributes& attributes)
	{
		assert(!m_groups.empty());
		m_groups.back().keywords.emplace_back(attributes);
	}

private:
	Groups m_groups;
};

} // namespace

} // namespace HomeCompa::Flibrary::UserData

namespace HomeCompa::Flibrary::UserData
{

std::unique_ptr<IRestorer> CreateGroupsRestorer7()
{
	return std::make_unique<GroupsRestorer>();
}

}
