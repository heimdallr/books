#include "fnd/FindPair.h"
#include "fnd/ScopedCall.h"

#include "database/interface/IDatabase.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/ProductConstant.h"

#include "model/AuthorsModel.h"
#include "util/xml/XmlAttributes.h"

#include "restore.h"

namespace HomeCompa::Flibrary::UserData {

namespace {

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
		return !m_query->Eof() ? std::optional { std::make_pair(m_query->Get<long long>(0), QString{}) } : std::nullopt;
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

class FavoritesRestorer final : public IRestorer
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

} // namespace

std::unique_ptr<IRestorer> CreateFavoritesRestorer10()
{
	return std::make_unique<FavoritesRestorer>();
}

} // namespace HomeCompa::Flibrary::UserData
