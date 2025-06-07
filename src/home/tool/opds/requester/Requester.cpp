#include "Requester.h"

#include <ranges>

#include <QBuffer>
#include <QByteArray>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

#include "fnd/FindPair.h"
#include "fnd/IsOneOf.h"
#include "fnd/ScopedCall.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITemporaryTable.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/Enums.h"
#include "interface/constants/GenresLocalization.h"
#include "interface/constants/Localization.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IAnnotationController.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseController.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IScriptController.h"

#include "logic/data/DataItem.h"
#include "logic/data/Genre.h"
#include "util/AnnotationControllerObserver.h"
#include "util/Fb2InpxParser.h"
#include "util/SortString.h"
#include "util/localization.h"
#include "util/timer.h"
#include "util/xml/XmlWriter.h"

#include "log.h"
#include "root.h"

namespace HomeCompa::Opds
{
#define OPDS_REQUEST_ROOT_ITEM(NAME) QByteArray PostProcess_##NAME(const IPostProcessCallback& callback, QIODevice& stream, ContentType contentType, const QStringList&);
OPDS_REQUEST_ROOT_ITEMS_X_MACRO
#undef OPDS_REQUEST_ROOT_ITEM

#define OPDS_REQUEST_ROOT_ITEM(NAME) std::unique_ptr<Flibrary::IAnnotationController::IStrategy> CreateAnnotationControllerStrategy_##NAME(const ISettings&);
OPDS_REQUEST_ROOT_ITEMS_X_MACRO
#undef OPDS_REQUEST_ROOT_ITEM

}

using namespace HomeCompa;
using namespace Opds;

namespace
{

constexpr std::pair<const char*, QByteArray (*)(const IPostProcessCallback&, QIODevice&, ContentType, const QStringList&)> POSTPROCESSORS[] {
#define OPDS_REQUEST_ROOT_ITEM(NAME) { "/" #NAME, &PostProcess_##NAME },
	OPDS_REQUEST_ROOT_ITEMS_X_MACRO
#undef OPDS_REQUEST_ROOT_ITEM
};

constexpr std::pair<const char*, std::unique_ptr<Flibrary::IAnnotationController::IStrategy> (*)(const ISettings&)> ANNOTATION_CONTROLLER_STRATEGY_CREATORS[] {
#define OPDS_REQUEST_ROOT_ITEM(NAME) { "/" #NAME, &CreateAnnotationControllerStrategy_##NAME },
	OPDS_REQUEST_ROOT_ITEMS_X_MACRO
#undef OPDS_REQUEST_ROOT_ITEM
};

constexpr auto CONTEXT = "Requester";
constexpr auto TOTAL = QT_TRANSLATE_NOOP("Requester", "Total: %1");
constexpr auto BOOKS = QT_TRANSLATE_NOOP("Requester", "Books: %1");
constexpr auto STARTS_WITH_COUNT = QT_TRANSLATE_NOOP("Requester", "%1 with '%2'~: %3");

constexpr auto BOOK = QT_TRANSLATE_NOOP("Requester", "Book");
constexpr auto SEARCH_RESULTS = QT_TRANSLATE_NOOP("Requester", R"(Books found for the request "%1": %2)");
constexpr auto NOTHING_FOUND = QT_TRANSLATE_NOOP("Requester", R"(No books found for the request "%1")");
constexpr auto PREVIOUS = QT_TRANSLATE_NOOP("Requester", "[Previous page]");
constexpr auto NEXT = QT_TRANSLATE_NOOP("Requester", "[Next page]");
constexpr auto FIRST = QT_TRANSLATE_NOOP("Requester", "[First page]");
constexpr auto LAST = QT_TRANSLATE_NOOP("Requester", "[Last page]");

constexpr auto ENTRY = "entry";
constexpr auto TITLE = "title";
constexpr auto CONTENT = "content";
constexpr auto OPDS_BOOK_LIMIT_KEY = "opds/BookEntryLimit";
constexpr auto OPDS_BOOK_LIMIT_DEFAULT = 25;

TR_DEF

constexpr auto AUTHOR_COUNT = "with WithTable(Id) as (select distinct l.AuthorID from Author_List l %1) select '%2', count(42) from WithTable";
constexpr auto SERIES_COUNT = "with WithTable(Id) as (select distinct l.SeriesID from Series_List l %1) select '%2', count(42) from WithTable";
constexpr auto GENRE_COUNT = "with WithTable(Id) as (select distinct l.GenreCode from Genre_List l %1) select '%2', count(42) from WithTable";
constexpr auto KEYWORD_COUNT = "with WithTable(Id) as (select distinct l.KeywordID from Keyword_List l %1) select '%2', count(42) from WithTable";
constexpr auto UPDATE_COUNT = "with WithTable(Id) as (select distinct l.UpdateID from Books l  %1) select '%2', count(42) from WithTable";
constexpr auto FOLDER_COUNT = "with WithTable(Id) as (select distinct l.FolderID from Books l  %1) select '%2', count(42) from WithTable";

constexpr auto AUTHOR_JOIN_PARAMETERS = "join Author_List al on al.BookID = l.BookID and al.AuthorID = %1";
constexpr auto SERIES_JOIN_PARAMETERS = "join Series_List sl on sl.BookID = l.BookID and sl.SeriesID = %1";

constexpr auto AUTHOR_JOIN_SELECT = "join Author_List l on l.AuthorID = a.AuthorID";
constexpr auto SERIES_JOIN_SELECT = "join Series_List l on l.SeriesID = s.SeriesID";

constexpr auto AUTHOR_SELECT = "select a.LastName || coalesce(' ' || nullif(a.FirstName, '') || coalesce(' ' || nullif(a.middleName, ''), ''), '') from Authors a where a.AuthorID = ?";
constexpr auto SERIES_SELECT = "select s.SeriesTitle from Series s where s.SeriesID = ?";

constexpr auto AUTHOR_COUNT_STARTS_WITH = R"(
select count(distinct a.AuthorID) 
from Authors a 
%3 
where a.IsDeleted != %1 and (a.SearchName = :starts or a.SearchName like :starts_like||'%' ESCAPE '%2')
)";

constexpr auto SERIES_COUNT_STARTS_WITH = R"(
select count(distinct s.SeriesID) 
from Series s 
%3 
where s.IsDeleted != %1 and (s.SearchTitle = :starts or s.SearchTitle like :starts_like||'%' ESCAPE '%2')
)";

constexpr auto AUTHOR_STARTS_WITH = R"(
select substr(a.SearchName, 1, :length), count(distinct a.AuthorID) 
from Authors a 
%3 
where a.IsDeleted != %1 and (a.SearchName = :starts or a.SearchName like :starts_like||'%' ESCAPE '%2')
group by substr(a.SearchName, 1, :length)
)";

constexpr auto SERIES_STARTS_WITH = R"(
select substr(s.SearchTitle, 1, :length), count(distinct s.SeriesID) 
from Series s 
%3 
where s.IsDeleted != %1 and (s.SearchTitle = :starts or s.SearchTitle like :starts_like||'%' ESCAPE '%2')
group by substr(s.SearchTitle, 1, :length)
)";

constexpr auto AUTHOR_SELECT_SINGLE = R"(
select distinct a.AuthorID, a.LastName || coalesce(' ' || nullif(a.FirstName, '') || coalesce(' ' || nullif(a.middleName, ''), ''), '')
from Authors a 
%3 
where a.IsDeleted != %1 and (a.SearchName = :starts or a.SearchName like :starts_like||'%' ESCAPE '%2')
)";

constexpr auto SERIES_SELECT_SINGLE = R"(
select distinct s.SeriesID, s.SeriesTitle
from Series s 
%3 
where s.IsDeleted != %1 and (s.SearchTitle = :starts or s.SearchTitle like :starts_like||'%' ESCAPE '%2')
)";

constexpr auto AUTHOR_SELECT_EQUAL = R"(
select distinct a.AuthorID, a.LastName || coalesce(' ' || nullif(a.FirstName, '') || coalesce(' ' || nullif(a.middleName, ''), ''), '')
from Authors a 
%2 
where a.IsDeleted != %1 and a.SearchName = :starts
)";

constexpr auto SERIES_SELECT_EQUAL = R"(
select distinct s.SeriesID, s.SeriesTitle
from Series s 
%2 
where s.IsDeleted != %1 and s.SearchTitle = :starts
)";

struct NavigationDescription
{
	const char* type { nullptr };
	const char* count { nullptr };
	const char* joinParameters { nullptr };
	const char* joinSelect { nullptr };
	const char* select { nullptr };
	const char* countStartsWith { nullptr };
	const char* startsWith { nullptr };
	const char* selectSingle { nullptr };
	const char* selectEqual { nullptr };
};

// clang-format off
constexpr NavigationDescription NAVIGATION_DESCRIPTION[] {
	{ Loc::Authors  , AUTHOR_COUNT, AUTHOR_JOIN_PARAMETERS, AUTHOR_JOIN_SELECT, AUTHOR_SELECT, AUTHOR_COUNT_STARTS_WITH, AUTHOR_STARTS_WITH, AUTHOR_SELECT_SINGLE, AUTHOR_SELECT_EQUAL },
	{ Loc::Series   , SERIES_COUNT, SERIES_JOIN_PARAMETERS, SERIES_JOIN_SELECT, SERIES_SELECT, SERIES_COUNT_STARTS_WITH, SERIES_STARTS_WITH, SERIES_SELECT_SINGLE, SERIES_SELECT_EQUAL },
	{ Loc::Genres   , GENRE_COUNT },
	{ Loc::Keywords , KEYWORD_COUNT },
	{ Loc::Updates  , UPDATE_COUNT },
	{ Loc::Archives , FOLDER_COUNT },
	{ Loc::Languages },
	{ Loc::Groups },
	{ Loc::Search },
	{ Loc::AllBooks },
};
// clang-format on

static_assert(std::size(NAVIGATION_DESCRIPTION) == static_cast<size_t>(Flibrary::NavigationMode::Last));

struct Node
{
	using Attributes = std::vector<std::pair<QString, QString>>;
	using Children = std::vector<Node>;
	QString name;
	QString value;
	Attributes attributes;
	Children children;

	QString title;

	QString author;
	QString series;
	int seqNum;
};

const QString& GetContent(const Node& node)
{
	const auto it = std::ranges::find(node.children, CONTENT, [](const auto& item) { return item.name; });
	assert(it != node.children.end());
	return it->value;
}

std::tuple<Util::QStringWrapper, Util::QStringWrapper, int, Util::QStringWrapper> ToComparable(const Node& node)
{
	return node.author.isEmpty() ? std::make_tuple(Util::QStringWrapper { GetContent(node) }, Util::QStringWrapper { node.title }, 0, Util::QStringWrapper { node.author })
	                             : std::make_tuple(Util::QStringWrapper { node.author }, Util::QStringWrapper { node.series }, node.seqNum, Util::QStringWrapper { node.title });
}

bool operator<(const Node& lhs, const Node& rhs)
{
	return ToComparable(lhs) < ToComparable(rhs);
}

std::vector<Node> GetStandardNodes(QString id, QString title)
{
	return std::vector<Node> {
		{ "updated", QDateTime::currentDateTime().toUTC().toString("yyyy-MM-ddThh:mm:ssZ") },
		{      "id",														 std::move(id) },
		{     TITLE,													  std::move(title) },
	};
}

Util::XmlWriter& operator<<(Util::XmlWriter& writer, const Node& node)
{
	const auto nodeGuard = writer.Guard(node.name);
	std::ranges::for_each(node.attributes, [&](const auto& item) { writer.WriteAttribute(item.first, item.second); });
	writer.WriteCharacters(node.value);
	std::ranges::for_each(node.children, [&](const auto& item) { writer << item; });
	return writer;
}

std::pair<QString, char> PrepareForLike(QString arg)
{
	static constexpr char ESCAPE[] { '\\', '|', '#', '@', '~', '^' };
	const auto it = std::ranges::find_if(ESCAPE, [&](const auto ch) { return !arg.contains(ch); });
	assert(it != std::end(ESCAPE));
	arg.replace('_', QString("%1_").arg(*it));
	arg.replace('%', QString("%1%").arg(*it));
	return std::make_pair(std::move(arg), *it);
}

Node GetHead(QString id, QString title, QString root, QString self)
{
	auto standardNodes = GetStandardNodes(std::move(id), std::move(title));
	standardNodes.emplace_back("link",
	                           QString {
    },
	                           Node::Attributes { { "href", root }, { "rel", "start" }, { "type", "application/atom+xml;profile=opds-catalog;kind=navigation" } });
	standardNodes.emplace_back("link",
	                           QString {
    },
	                           Node::Attributes { { "href", std::move(self) }, { "rel", "self" }, { "type", "application/atom+xml;profile=opds-catalog;kind=navigation" } });
	standardNodes.emplace_back("link",
	                           QString {
    },
	                           Node::Attributes { { "href", QString("%1/opensearch").arg(root) }, { "rel", "search" }, { "type", "application/opensearchdescription+xml" } });
	standardNodes.emplace_back("link",
	                           QString {
    },
	                           Node::Attributes { { "href", QString("%1/search?q={searchTerms}").arg(root) }, { "rel", "search" }, { "type", "application/atom+xml" } });
	return Node {
		"feed",
		{},
		{
          { "xmlns", "http://www.w3.org/2005/Atom" },
          { "xmlns:dc", "http://purl.org/dc/terms/" },
          { "xmlns:opds", "http://opds-spec.org/2010/catalog" },
		  },
		std::move(standardNodes)
	};
}

QString GetHref(const QString& root, const QString& path, const IRequester::Parameters& parameters)
{
	QStringList list;
	list.reserve(static_cast<int>(parameters.size()));
	std::ranges::transform(parameters, std::back_inserter(list), [](const auto& item) { return QString("%1=%2").arg(item.first, item.second); });
	return QString("%1%2%3").arg(root, path.isEmpty() ? QString {} : "/" + path, list.isEmpty() ? QString {} : "?" + list.join('&'));
}

Node& WriteEntry(Node::Children& children, const QString& root, const QString& path, const IRequester::Parameters& parameters, QString id, QString title, QString content = {}, const bool isCatalog = true)
{
	auto& entry = children.emplace_back(ENTRY, QString {}, Node::Attributes {}, GetStandardNodes(std::move(id), title));
	entry.title = std::move(title);

	if (isCatalog)
	{
		const auto href = GetHref(root, path, parameters);
		entry.children.emplace_back("link",
		                            QString {
        },
		                            Node::Attributes { { "href", QUrl(href).toString(QUrl::FullyEncoded) }, { "rel", "subsection" }, { "type", "application/atom+xml;profile=opds-catalog;kind=navigation" } });
	}
	if (!content.isEmpty())
		entry.children.emplace_back(CONTENT,
		                            std::move(content),
		                            Node::Attributes {
										{ "type", "text/html" }
        });

	return entry;
}

QString CreateSelf(const QString& root, const QString& path, const IRequester::Parameters& parameters)
{
	QStringList list;
	list.reserve(static_cast<int>(parameters.size()));
	std::ranges::transform(parameters, std::back_inserter(list), [](const auto& item) { return QString("%1=%2").arg(item.first, item.second); });
	return QString("%1%2%3").arg(root, path.isEmpty() ? QString {} : QString("/%1").arg(path), list.isEmpty() ? QString {} : list.join('&'));
}

QString GetParameter(const IRequester::Parameters& parameters, const QString& key)
{
	const auto it = parameters.find(key);
	return it != parameters.end() ? it->second : QString {};
}

QString GetJoin(const IRequester::Parameters& parameters)
{
	if (parameters.empty())
		return {};

	QStringList list;
	std::ranges::transform(NAVIGATION_DESCRIPTION | std::views::filter([&](const auto& item) { return item.joinParameters && parameters.contains(item.type); }),
	                       std::back_inserter(list),
	                       [&](const auto& item) { return QString(item.joinParameters).arg(parameters.at(item.type)); });
	if (list.isEmpty())
		return {};

	list << "join Books_View b on b.BookID = l.BookID and b.IsDeleted != %1";

	return list.join("\n");
}

struct TitleHelper
{
	QString defaultTitle;
	QString additionalTitle;
};

QString GetTitle(DB::IDatabase& db, const IRequester::Parameters& parameters, TitleHelper helper)
{
	QStringList list;
	std::ranges::transform(NAVIGATION_DESCRIPTION | std::views::filter([&](const auto& item) { return item.select && parameters.contains(item.type); }),
	                       std::back_inserter(list),
	                       [&](const auto& item)
	                       {
							   const auto query = db.CreateQuery(item.select);
							   query->Bind(0, parameters.at(item.type).toStdString());
							   query->Execute();
							   assert(!query->Eof());
							   return QString(query->template Get<const char*>(0));
						   });
	if (!helper.additionalTitle.isEmpty())
		list.emplaceBack(std::move(helper.additionalTitle));

	return list.isEmpty() ? std::move(helper.defaultTitle) : list.join(", ");
}

} // namespace

class Requester::Impl final : public IPostProcessCallback
{
private: // IPostProcessCallback
	QString GetFileName(const QString& bookId) const override
	{
		return m_bookExtractor->GetFileName(bookId);
	}

	std::pair<QString, std::vector<QByteArray>> GetAuthorInfo(const QString& name) const override
	{
		if (auto info = m_authorAnnotationController->GetInfo(name); !info.isEmpty())
			return std::make_pair(std::move(info), m_authorAnnotationController->GetImages(name));

		return {};
	}

public:
	Impl(std::shared_ptr<const ISettings> settings,
	     std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
	     std::shared_ptr<const Flibrary::IDatabaseController> databaseController,
	     std::shared_ptr<const Flibrary::IAuthorAnnotationController> authorAnnotationController,
	     std::shared_ptr<const ICoverCache> coverCache,
	     std::shared_ptr<const IBookExtractor> bookExtractor,
	     std::shared_ptr<const INoSqlRequester> noSqlRequester,
	     std::shared_ptr<Flibrary::IAnnotationController> annotationController)
		: m_settings { std::move(settings) }
		, m_collectionProvider { std::move(collectionProvider) }
		, m_databaseController { std::move(databaseController) }
		, m_authorAnnotationController { std::move(authorAnnotationController) }
		, m_coverCache { std::move(coverCache) }
		, m_bookExtractor { std::move(bookExtractor) }
		, m_noSqlRequester { std::move(noSqlRequester) }
		, m_annotationController { std::move(annotationController) }
	{
	}

	const Flibrary::ICollectionProvider& GetCollectionProvider() const
	{
		return *m_collectionProvider;
	}

	Node GetRoot(const QString& root, const Parameters& parameters) const
	{
		const auto removedFlag = GetRemovedFlag();
		const auto db = m_databaseController->GetDatabase(true);

		static constexpr std::pair<Flibrary::NavigationMode, const char*> navigationTypes[] {
#define OPDS_ROOT_ITEM(NAME) { Flibrary::NavigationMode::NAME, Loc::NAME },
			OPDS_ROOT_ITEMS_X_MACRO
#undef OPDS_ROOT_ITEM
		};

		auto head = GetHead("root", GetTitle(*db, parameters, { .defaultTitle = m_collectionProvider->GetActiveCollection().name }), root, CreateSelf(root, "", parameters));

		auto join = GetJoin(parameters);
		if (!join.isEmpty())
			join = join.arg(removedFlag);

		QStringList dbStatQueryTextItems;
		std::ranges::transform(navigationTypes | std::views::filter([&](const auto& item) { return NAVIGATION_DESCRIPTION[static_cast<size_t>(item.first)].count && !parameters.contains(item.second); }),
		                       std::back_inserter(dbStatQueryTextItems),
		                       [&](const auto& item)
		                       {
								   const auto& d = NAVIGATION_DESCRIPTION[static_cast<size_t>(item.first)];
								   return QString(d.count).arg(join).arg(d.type);
							   });

		{
			for (const auto& dbStatQueryText : dbStatQueryTextItems)
			{
				const auto query = db->CreateQuery(dbStatQueryText.toStdString());
				query->Execute();
				assert(!query->Eof());
				const auto* id = query->Get<const char*>(0);
				if (const auto count = query->Get<int>(1))
					WriteEntry(head.children, root, id, parameters, id, Loc::Tr(Loc::NAVIGATION, id), Tr(TOTAL).arg(count));
			}
		}
		if (!parameters.contains(Loc::Groups))
		{
			const auto query = db->CreateQuery(QString(R"(
select g.GroupID, g.Title, count(42)
from Groups_User g 
join Groups_List_User l on l.GroupID = g.GroupID 
%2
where g.IsDeleted != %1 
group by g.GroupID
)")
			                                       .arg(removedFlag)
			                                       .arg(join)
			                                       .toStdString());
			for (query->Execute(); !query->Eof(); query->Next())
			{
				auto p = parameters;
				const auto id = QString("%1/%2").arg(Loc::Groups).arg(p[Loc::Groups] = query->Get<const char*>(0));
				WriteEntry(head.children, root, "", p, id, query->Get<const char*>(1), Tr(BOOKS).arg(query->Get<int>(2)));
			}
		}

		return head;
	}

	Node GetNavigation(const QString& root, const Parameters& parameters, const Flibrary::NavigationMode navigationMode) const
	{
		const auto removedFlag = GetRemovedFlag();
		const auto db = m_databaseController->GetDatabase(true);
		const auto& d = NAVIGATION_DESCRIPTION[static_cast<size_t>(navigationMode)];

		ptrdiff_t equalCount = 0;
		QStringList equal;
		std::vector<std::tuple<long long, QString, long long>> ones;
		std::multimap<long long, QString, std::greater<>> buffer;

		const auto startsWithGlobal = GetParameter(parameters, "starts");
		auto join = GetJoin(parameters);
		if (!join.isEmpty())
			join = QString("%1\n%2").arg(d.joinSelect, join.arg(removedFlag));

		const auto countTotal = [&]
		{
			const auto [startsWithLike, escape] = PrepareForLike(startsWithGlobal);
			const auto queryText = QString(d.countStartsWith).arg(removedFlag).arg(escape).arg(join);
			const auto query = db->CreateQuery(queryText.toStdString());
			query->Bind(":starts", startsWithGlobal.toStdString());
			query->Bind(":starts_like", startsWithLike.toStdString());
			query->Execute();
			assert(!query->Eof());
			return query->Get<long long>(0);
		}();

		const auto selectOne = [&](const QString& queryText, const std::vector<std::pair<QString, QString>>& params)
		{
			const auto query = db->CreateQuery(queryText.toStdString());
			for (const auto& [key, value] : params)
				query->Bind(key.toStdString(), value.toStdString());

			for (query->Execute(); !query->Eof(); query->Next())
				ones.emplace_back(query->Get<long long>(0), query->Get<const char*>(1), 0);
		};

		const auto maxSize = GetMaxResultSize();
		if (countTotal <= maxSize)
		{
			const auto [startsWithLike, escape] = PrepareForLike(startsWithGlobal);
			const auto queryText = QString(d.selectSingle).arg(removedFlag).arg(escape).arg(join);
			selectOne(queryText,
			          {
						  {      ":starts", startsWithGlobal },
						  { ":starts_like",   startsWithLike },
            });
		}
		else
		{
			const auto selectStarts = [&](const auto startsWith)
			{
				const auto [startsWithLike, escape] = PrepareForLike(startsWith);
				const auto queryText = QString(d.startsWith).arg(removedFlag).arg(escape).arg(join);
				const auto query = db->CreateQuery(queryText.toStdString());
				query->Bind(":length", startsWith.length() + 1);
				query->Bind(":starts", startsWith.toStdString());
				query->Bind(":starts_like", startsWithLike.toStdString());
				for (query->Execute(); !query->Eof(); query->Next())
				{
					QString s = query->template Get<const char*>(0);
					const auto count = query->template Get<long long>(1);
					if (s == startsWith)
					{
						equal.emplaceBack(std::move(s));
						equalCount += count;
					}
					else
					{
						buffer.emplace(count, std::move(s));
					}
				}
			};

			selectStarts(startsWithGlobal);

			while (!buffer.empty() && equalCount + static_cast<ptrdiff_t>(buffer.size()) < maxSize)
			{
				auto it = buffer.begin();
				const auto startsWith = std::move(it->second);
				buffer.erase(it);
				selectStarts(startsWith);
			}
		}

		auto head = GetHead(d.type, GetTitle(*db, parameters, { .additionalTitle = Loc::Tr(Loc::NAVIGATION, d.type) }), root, CreateSelf(root, d.type, parameters));

		for (auto&& [count, startsWith] : buffer)
		{
			Parameters p = parameters;
			const auto& s = (p["starts"] = std::move(startsWith));
			WriteEntry(head.children, root, d.type, p, QString("%1/starts/%2").arg(d.type, s), QString("%1~").arg(s), QString::number(count));
		}

		Parameters typedParameters = parameters;
		typedParameters.erase("starts");

		for (const auto& s : equal)
		{
			const auto queryText = QString(d.selectEqual).arg(removedFlag).arg(join);
			selectOne(queryText,
			          {
						  { ":starts", s },
            });
		}

		for (auto&& [navigationId, title, count] : ones)
		{
			auto id = (typedParameters[d.type] = QString::number(navigationId));
			WriteEntry(head.children, root, "", typedParameters, QString("%1/%2").arg(d.type, id), std::move(title), Tr(BOOKS).arg(count));
		}

		const auto entryBegin = std::ranges::find(head.children, ENTRY, [](const auto& item) { return item.name; });
		std::sort(entryBegin, head.children.end(), [](const auto& lhs, const auto& rhs) { return Util::QStringWrapper { lhs.title } < Util::QStringWrapper { rhs.title }; });

		return head;
	}

private:
	int GetRemovedFlag() const
	{
		return ShowRemoved() ? 2 : 1;
	}

	bool ShowRemoved() const
	{
		return m_settings->Get(Flibrary::Constant::Settings::SHOW_REMOVED_BOOKS_KEY, false);
	}

	ptrdiff_t GetMaxResultSize() const
	{
		if (const auto result = m_settings->Get(OPDS_BOOK_LIMIT_KEY, OPDS_BOOK_LIMIT_DEFAULT); result > 0)
			return result;
		return OPDS_BOOK_LIMIT_DEFAULT;
	}

private:
	std::shared_ptr<const ISettings> m_settings;
	std::shared_ptr<const Flibrary::ICollectionProvider> m_collectionProvider;
	std::shared_ptr<const Flibrary::IDatabaseController> m_databaseController;
	std::shared_ptr<const Flibrary::IAuthorAnnotationController> m_authorAnnotationController;
	std::shared_ptr<const ICoverCache> m_coverCache;
	std::shared_ptr<const IBookExtractor> m_bookExtractor;
	std::shared_ptr<const INoSqlRequester> m_noSqlRequester;
	std::shared_ptr<Flibrary::IAnnotationController> m_annotationController;
};

namespace
{

QByteArray PostProcess(const ContentType contentType, const QString& root, const IPostProcessCallback& callback, QByteArray& src, const QStringList& parameters)
{
	if (root.isEmpty())
		return src;

	QBuffer buffer(&src);
	buffer.open(QIODevice::ReadOnly);
	const auto postprocessor = FindSecond(POSTPROCESSORS, root.toStdString().data(), PszComparer {});
	auto result = postprocessor(callback, buffer, contentType, parameters);

#ifndef NDEBUG
	PLOGV << result;
#endif

	return result;
}

template <typename Obj, typename NavigationGetter, typename... ARGS>
QByteArray GetImpl(const Obj& obj, NavigationGetter getter, const ContentType contentType, const QString& root, const IRequester::Parameters& parameters, const ARGS&... args)
{
	if (!obj.GetCollectionProvider().ActiveCollectionExists())
		return {};

	QByteArray bytes;
	QBuffer buffer(&bytes);
	try
	{
		const ScopedCall bufferGuard([&] { buffer.open(QIODevice::WriteOnly); }, [&] { buffer.close(); });
		const auto node = std::invoke(getter, std::cref(obj), std::cref(root), std::cref(parameters), std::cref(args)...);
		Util::XmlWriter writer(buffer);
		writer << node;
	}
	catch (const std::exception& ex)
	{
		PLOGE << ex.what();
		return {};
	}
	catch (...)
	{
		PLOGE << "Unknown error";
		return {};
	}
	buffer.close();

#ifndef NDEBUG
	PLOGV << bytes;
#endif

	return PostProcess(contentType, root, obj, bytes, { root });
}

} // namespace

Requester::Requester(std::shared_ptr<const ISettings> settings,
                     std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
                     std::shared_ptr<const Flibrary::IDatabaseController> databaseController,
                     std::shared_ptr<const Flibrary::IAuthorAnnotationController> authorAnnotationController,
                     std::shared_ptr<const ICoverCache> coverCache,
                     std::shared_ptr<const IBookExtractor> bookExtractor,
                     std::shared_ptr<const INoSqlRequester> noSqlRequester,
                     std::shared_ptr<Flibrary::IAnnotationController> annotationController)
	: m_impl(std::move(settings),
             std::move(collectionProvider),
             std::move(databaseController),
             std::move(authorAnnotationController),
             std::move(coverCache),
             std::move(bookExtractor),
             std::move(noSqlRequester),
             std::move(annotationController))
{
	PLOGV << "Requester created";
}

Requester::~Requester()
{
	PLOGV << "Requester destroyed";
}

QByteArray Requester::GetRoot(const QString& root, const Parameters& parameters) const
{
	return GetImpl(*m_impl, &Impl::GetRoot, ContentType::Root, std::cref(root), std::cref(parameters));
}

#define OPDS_ROOT_ITEM(NAME)                                                                                                                            \
	QByteArray Requester::Get##NAME(const QString& root, const Parameters& parameters) const                                                            \
	{                                                                                                                                                   \
		return GetImpl(*m_impl, &Impl::GetNavigation, ContentType::Navigation, std::cref(root), std::cref(parameters), Flibrary::NavigationMode::NAME); \
	}
OPDS_ROOT_ITEMS_X_MACRO
#undef OPDS_ROOT_ITEM
