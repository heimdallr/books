#include "fnd/FindPair.h"
#include "fnd/ScopedCall.h"

#include "database/interface/IDatabase.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/ProductConstant.h"
#include "interface/logic/IDatabaseProvider.h"
#include "interface/logic/IFilterProvider.h"

#include "constants/books.h"
#include "constants/groups.h"
#include "model/AuthorsModel.h"
#include "util/xml/XmlAttributes.h"

#include "restore.h"
#include "constants/filter.h"

namespace HomeCompa::Flibrary::UserData {

namespace Favorites { namespace {

struct ICodeGetter // NOLINT(cppcoreguidelines-special-member-functions)
{
	virtual ~ICodeGetter()                                                         = default;
	virtual std::optional<std::pair<long long, QString>> Get(const QString&) const = 0;
};

struct CodeGetterStub : ICodeGetter
{
	static std::unique_ptr<ICodeGetter> Create(DB::IDatabase&)
	{
		return std::make_unique<CodeGetterStub>();
	}

	std::optional<std::pair<long long, QString>> Get(const QString& /*title*/) const override
	{
		assert(false);
		return {};
	}
};

struct QueryCodeGetter : ICodeGetter
{
	QueryCodeGetter(DB::IDatabase& db, const std::string_view queryText, const bool codeMode = false)
		: m_query { db.CreateQuery(queryText) }
		, m_codeMode { codeMode }
	{
	}

	std::optional<std::pair<long long, QString>> Get(const QString& title) const override
	{
		const ScopedCall _([this] {
			m_query->Reset();
		});

		m_query->Execute(title);
		if (m_query->Eof())
			return {};

		return m_codeMode ? std::make_pair(-1LL, m_query->Get<QString>(0)) : std::make_pair(m_query->Get<long long>(0), QString {});
	}

protected:
	std::unique_ptr<DB::IQuery> m_query;

private:
	const bool m_codeMode;
};

struct AuthorsCodeGetter final : QueryCodeGetter
{
	static std::unique_ptr<ICodeGetter> Create(DB::IDatabase& db)
	{
		return std::make_unique<AuthorsCodeGetter>(db);
	}

	explicit AuthorsCodeGetter(DB::IDatabase& db)
		: QueryCodeGetter(db, "select AuthorID from Authors where LastName = ? and FirstName = ? and MiddleName = ? and NickName = ?")
	{
	}

	std::optional<std::pair<long long, QString>> Get(const QString& title) const override
	{
		const auto list = title.split(',', Qt::KeepEmptyParts);
		m_query->Execute(list[0], list[1], list[2], !list[3].isEmpty() ? std::optional { list[3] } : std::nullopt);
		return !m_query->Eof() ? std::optional { std::make_pair(m_query->Get<long long>(0), QString {}) } : std::nullopt;
	}
};

struct SeriesCodeGetter final : QueryCodeGetter
{
	static std::unique_ptr<ICodeGetter> Create(DB::IDatabase& db)
	{
		return std::make_unique<SeriesCodeGetter>(db);
	}

	explicit SeriesCodeGetter(DB::IDatabase& db)
		: QueryCodeGetter(db, "select SeriesID from Series where SeriesTitle = ?")
	{
	}
};

struct GenresCodeGetter final : QueryCodeGetter
{
	static std::unique_ptr<ICodeGetter> Create(DB::IDatabase& db)
	{
		return std::make_unique<GenresCodeGetter>(db);
	}

	explicit GenresCodeGetter(DB::IDatabase& db)
		: QueryCodeGetter(db, "select GenreCode from Genres where FB2Code = ?", true)
	{
	}
};

struct KeywordsCodeGetter final : QueryCodeGetter
{
	static std::unique_ptr<ICodeGetter> Create(DB::IDatabase& db)
	{
		return std::make_unique<KeywordsCodeGetter>(db);
	}

	explicit KeywordsCodeGetter(DB::IDatabase& db)
		: QueryCodeGetter(db, "select KeywordID from Keywords where KeywordTitle = ?")
	{
	}
};

struct PassThruCodeGetter : ICodeGetter
{
	explicit PassThruCodeGetter(const bool codeMode = false)
		: m_codeMode { codeMode }
	{
	}

	std::optional<std::pair<long long, QString>> Get(const QString& title) const override
	{
		if (m_codeMode)
			return std::make_pair(-1LL, title);

		bool       ok    = false;
		const auto value = title.toLongLong(&ok);
		return ok ? std::optional { std::make_pair(value, QString {}) } : std::nullopt;
	}

private:
	const bool m_codeMode;
};

struct PublishYearCodeGetter final : PassThruCodeGetter
{
	static std::unique_ptr<ICodeGetter> Create(DB::IDatabase&)
	{
		return std::make_unique<PublishYearCodeGetter>();
	}
};

struct LanguagesCodeGetter final : PassThruCodeGetter
{
	static std::unique_ptr<ICodeGetter> Create(DB::IDatabase&)
	{
		return std::make_unique<LanguagesCodeGetter>();
	}

	LanguagesCodeGetter()
		: PassThruCodeGetter(true)
	{
	}
};

using CodeGetterCreator = std::unique_ptr<ICodeGetter> (*)(DB::IDatabase&);

#define CODE_GETTER_CREATOR_STUB_ITEMS_X_MACRO \
CODE_GETTER_CREATOR_STUB_ITEM(Updates)         \
CODE_GETTER_CREATOR_STUB_ITEM(Archives)        \
CODE_GETTER_CREATOR_STUB_ITEM(Groups)          \
CODE_GETTER_CREATOR_STUB_ITEM(Favorites)       \
CODE_GETTER_CREATOR_STUB_ITEM(Search)          \
CODE_GETTER_CREATOR_STUB_ITEM(Reviews)         \
CODE_GETTER_CREATOR_STUB_ITEM(AlreadyRead)     \
CODE_GETTER_CREATOR_STUB_ITEM(History)         \
CODE_GETTER_CREATOR_STUB_ITEM(AllBooks)

#define CODE_GETTER_CREATOR_STUB_ITEM(NAME) struct NAME##CodeGetter final : CodeGetterStub{};
CODE_GETTER_CREATOR_STUB_ITEMS_X_MACRO
#undef CODE_GETTER_CREATOR_STUB_ITEM

constexpr CodeGetterCreator CODE_GETTER_CREATORS[] {
#define NAVIGATION_MODE_ITEM(NAME) &NAME##CodeGetter::Create,
	NAVIGATION_MODE_ITEMS_X_MACRO
#undef NAVIGATION_MODE_ITEM
};

class Restorer final : public IRestorer
{
private: // IRestorer
	void AddElement(const QString& name, const Util::XmlAttributes& attributes) override
	{
		if (name == Constant::ITEM)
		{
			assert(!m_data.empty());
			m_data.back().second.emplace_back(attributes.GetAttribute(Constant::TITLE).toString());
			return;
		}

		m_data.emplace_back(FindSecond(NAVIGATION_NAMES, name.toStdString().data(), PszComparer {}), decltype(m_data)::value_type::second_type {});
	}

	void Restore(DB::IDatabaseWritable& db) const override
	{
		std::vector<std::tuple<int, long long, QString>> toInsert;
		for (const auto& [navigationMode, values] : m_data)
		{
			const auto index = static_cast<size_t>(navigationMode);
			assert(index < std::size(CODE_GETTER_CREATORS));
			const auto codeGetter = CODE_GETTER_CREATORS[index](db);
			for (const auto& value : values)
				if (const auto code = codeGetter->Get(value))
					toInsert.emplace_back(static_cast<int>(navigationMode), code->first, code->second);
		}

		const auto tr = db.CreateTransaction();
		tr->CreateCommand("delete from Favorites")->Execute();
		const auto command = tr->CreateCommand("insert into Favorites(FavoriteID, ObjectID, ObjectCode) values(?, ?, ?)");
		for (const auto& [fId, id, code] : toInsert)
		{
			command->Bind(0, fId);
			if (code.isEmpty())
				command->Bind(1, id);
			else
				command->Bind(2, code);
			command->Execute();
		}
		tr->Commit();
	}

private:
	std::vector<std::pair<NavigationMode, std::vector<QString>>> m_data;
};

}} // namespace Favorites

namespace Groups { namespace {

std::optional<QString> GetStringParameter(QString value)
{
	return !value.isEmpty() ? std::optional { std::move(value) } : std::nullopt;
}

class Restorer final : public IRestorer
{
	struct Item
	{
		QString title;
		QString createdAt;

		explicit Item(const Util::XmlAttributes& attributes)
			: title { attributes.GetAttribute(Constant::TITLE).toString() }
			, createdAt { attributes.GetAttribute(Constant::UserData::Books::CreatedAt).toString() }
		{
		}

		void Write(DB::ICommand& command, const long long groupId) const
		{
			command.Execute(groupId, GetStringParameter(createdAt), GetStringParameter(title));
		}
	};

	using Items = std::vector<Item>;

	struct Book
	{
		QString file;
		QString folder;
		QString createdAt;

		explicit Book(const Util::XmlAttributes& attributes)
			: file { attributes.GetAttribute(Constant::UserData::Books::FileName).toString() }
			, folder { attributes.GetAttribute(Constant::UserData::Books::Folder).toString() }
			, createdAt { attributes.GetAttribute(Constant::UserData::Books::CreatedAt).toString() }
		{
		}

		void Write(DB::ICommand& command, const long long groupId) const
		{
			command.Execute(groupId, GetStringParameter(createdAt), GetStringParameter(folder), GetStringParameter(file));
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
			command.Execute(GetStringParameter(title), GetStringParameter(createdAt));
		}
	};

	using Groups   = std::vector<Group>;
	using GroupRef = std::reference_wrapper<const Group>;

private: // IRestorer
	void AddElement(const QString& name, const Util::XmlAttributes& attributes) override
	{
		using Functor = void (Restorer::*)(const Util::XmlAttributes&);
		constexpr std::pair<const char*, Functor> collectors[] {
#define ITEM(NAME) { #NAME, &Restorer::Add##NAME }
			ITEM(Group), ITEM(Item), ITEM(Author), ITEM(Series), ITEM(Keyword),
#undef ITEM
		};

		const auto invoker = FindSecond(collectors, name.toStdString().data(), PszComparer {});
		std::invoke(invoker, this, std::cref(attributes));
	}

	void Restore(DB::IDatabaseWritable& db) const override
	{
		static constexpr auto commandTextBooks    = "insert into Groups_List_User(GroupID, ObjectID, CreatedAt) "
													"select ?, BookID, ? "
													"from Books "
													"where FolderID = (select FolderID from Folders where FolderTitle = ?) and FileName = ?";
		static constexpr auto commandTextAuthors  = "insert into Groups_List_User(GroupID, ObjectID, CreatedAt) "
													"select ?, AuthorID, ? "
													"from Authors "
													"where LastName||','||FirstName||','||MiddleName||','||coalesce(NickName, '') = ?";
		static constexpr auto commandTextSeries   = "insert into Groups_List_User(GroupID, ObjectID, CreatedAt) "
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
		const auto                                  command = tr.CreateCommand("insert into Groups_User(Title, CreatedAt) values(?, ?)");
		std::ranges::transform(m_groups, std::back_inserter(groups), [&](const Group& group) {
			group.Write(*command);
			const auto getLastIdQuery = tr.CreateQuery(IDatabaseProvider::SELECT_LAST_ID_QUERY);
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

}} // namespace Groups

namespace Filter { namespace {

constexpr std::string_view FIELD_NAMES[] { "LastName||','||FirstName||','||MiddleName||','||coalesce(NickName, '')", "SeriesTitle", "FB2Code", "", "KeywordTitle", "", "", "LanguageCode", "Title", "", "", "", "", "", "" };
static_assert(static_cast<size_t>(NavigationMode::Last) == std::size(FIELD_NAMES));

using namespace Constant::UserData::Filter;

class Restorer final : public IRestorer
{
	using DataValues = std::vector<std::pair<QString, int>>;

private: // IRestorer
	void AddElement(const QString& name, const Util::XmlAttributes& attributes) override
	{
		if (name != Constant::ITEM)
			return (void)(m_dataValues = &m_data[name]);

		assert(m_dataValues);
		m_dataValues->emplace_back(attributes.GetAttribute(Title).toString(), attributes.GetAttribute(Flag).toInt());
	}

	void Restore(DB::IDatabaseWritable& db) const override
	{
		const auto tr    = db.CreateTransaction();
		auto       range = IFilterProvider::GetDescriptions();
		(void)std::ranges::for_each(range, [&](const IFilterProvider::FilteredNavigation& description) {
			const auto it = m_data.find(description.navigationTitle);
			if (it == m_data.end())
				return;

			const auto index = static_cast<size_t>(description.navigationMode);
			assert(!FIELD_NAMES[index].empty());
			tr->CreateCommand(std::format("update {} set Flags = 0", description.table))->Execute();

			for (const auto command = tr->CreateCommand(std::format("update {} set Flags = ? where {} = ?", description.table, FIELD_NAMES[index])); const auto& [title, flags] : it->second)
				command->Execute(flags, title);
		});
		tr->Commit();
	}

private:
	DataValues*                             m_dataValues { nullptr };
	std::unordered_map<QString, DataValues> m_data;
};

}}

std::unique_ptr<IRestorer> CreateFavoritesRestorer10()
{
	return std::make_unique<Favorites::Restorer>();
}

std::unique_ptr<IRestorer> CreateGroupsRestorer10()
{
	return std::make_unique<Groups::Restorer>();
}

std::unique_ptr<IRestorer> CreateFilterRestorer10()
{
	return std::make_unique<Filter::Restorer>();
}

} // namespace HomeCompa::Flibrary::UserData
