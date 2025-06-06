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

constexpr auto AUTHOR_COUNT_STARTS_WITH = R"(
select count(42) 
from Authors a 
where a.IsDeleted != %1 and (a.SearchName = :starts or a.SearchName like :starts_like||'%' ESCAPE '%2')
)";

constexpr auto AUTHOR_STARTS_WITH = R"(
select substr(a.SearchName, 1, :length), count(42) 
from Authors a 
where a.IsDeleted != %1 and (a.SearchName = :starts or a.SearchName like :starts_like||'%' ESCAPE '%2')
group by substr(a.SearchName, 1, :length)
)";

constexpr auto AUTHOR_SELECT_SINGLE = R"(
select a.AuthorID, a.LastName || coalesce(' ' || nullif(a.FirstName, '') || coalesce(' ' || nullif(a.middleName, ''), ''), '')
from Authors a 
where a.IsDeleted != %1 and (a.SearchName = :starts or a.SearchName like :starts_like||'%' ESCAPE '%2')
)";

constexpr auto AUTHOR_SELECT_EQUAL = R"(
select a.AuthorID, a.LastName || coalesce(' ' || nullif(a.FirstName, '') || coalesce(' ' || nullif(a.middleName, ''), ''), '')
from Authors a 
where a.IsDeleted != %1 and a.SearchName = :starts
)";

struct NavigationDescription
{
	const char* type;
	const char* countStartsWith;
	const char* startsWith;
	const char* selectSingle;
	const char* selectEqual;
};

constexpr NavigationDescription NAVIGATION_DESCRIPTION[] {
	{ Loc::Authors, AUTHOR_COUNT_STARTS_WITH, AUTHOR_STARTS_WITH, AUTHOR_SELECT_SINGLE, AUTHOR_SELECT_EQUAL },
	{ Loc::Series },
	{ Loc::Genres },
	{ Loc::Keywords },
	{ Loc::Updates },
	{ Loc::Archives },
	{ Loc::Languages },
	{ Loc::Groups },
	{ Loc::Search },
	{ Loc::AllBooks },
};

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
		auto head = GetHead("root", m_collectionProvider->GetActiveCollection().name, root, CreateSelf(root, "", parameters));

		static constexpr std::pair<Flibrary::NavigationMode, const char*> navigationTypes[] {
#define OPDS_ROOT_ITEM(NAME) { Flibrary::NavigationMode::NAME, Loc::NAME },
			OPDS_ROOT_ITEMS_X_MACRO
#undef OPDS_ROOT_ITEM
		};
		QStringList dbStatQueryTextItems;
		std::ranges::transform(navigationTypes | std::views::filter([&](const auto& item) { return item.first != Flibrary::NavigationMode::Groups && !parameters.contains(item.second); }),
		                       std::back_inserter(dbStatQueryTextItems),
		                       [this](const auto& item) { return QString("select '%1', count(42) from %1 where IsDeleted != %2").arg(item.second).arg(GetRemovedFlag()); });

		auto dbStatQueryText = dbStatQueryTextItems.join(" union all ");
		dbStatQueryText.replace("count(42) from Archives", "count(42) from Folders");

		const auto db = m_databaseController->GetDatabase(true);
		{
			const auto query = db->CreateQuery(dbStatQueryText.toStdString());
			for (query->Execute(); !query->Eof(); query->Next())
			{
				const auto* id = query->Get<const char*>(0);
				if (const auto count = query->Get<int>(1))
					WriteEntry(head.children, root, id, parameters, id, Loc::Tr(Loc::NAVIGATION, id), Tr(TOTAL).arg(count));
			}
		}
		{
			const auto query = db->CreateQuery(QString(R"(
select g.GroupID, g.Title, count(42)
from Groups_User g 
join Groups_List_User l on l.GroupID = g.GroupID 
join Books_View b on b.BookID = l.BookID 
where b.IsDeleted != %1 
group by g.GroupID
)")
			                                       .arg(GetRemovedFlag())
			                                       .toStdString());
			for (query->Execute(); !query->Eof(); query->Next())
			{
				const auto id = QString("%1/%2").arg(Loc::Groups).arg(query->Get<int>(0));
				WriteEntry(head.children, root, "", parameters, id, query->Get<const char*>(1), Tr(BOOKS).arg(query->Get<int>(2)));
			}
		}

		return head;
	}

	Node GetNavigation(const QString& root, const Parameters& parameters, const Flibrary::NavigationMode navigationMode) const
	{
		const auto db = m_databaseController->GetDatabase();
		const auto& d = NAVIGATION_DESCRIPTION[static_cast<size_t>(navigationMode)];

		int equalCount = 0;
		QStringList equal;
		QStringList single;
		std::multimap<int, QString> buffer;

		const auto startsWithGlobal = GetParameter(parameters, "starts");

		const auto countTotal = [&]
		{
			const auto [startsWithLike, escape] = PrepareForLike(startsWithGlobal);
			const auto queryText = QString(d.countStartsWith).arg(GetRemovedFlag()).arg(escape);
			const auto query = db->CreateQuery(queryText.toStdString());
			query->Bind(":starts", startsWithGlobal.toStdString());
			query->Bind(":starts_like", startsWithLike.toStdString());
			query->Execute();
			assert(!query->Eof());
			return query->Get<long long>(0);
		}();

		const auto maxSize = GetMaxResultSize();
		if (countTotal <= maxSize)
		{
			single.emplaceBack(startsWithGlobal);
		}
		else
		{
			const auto selectStarts = [&](const auto startsWith)
			{
				const auto [startsWithLike, escape] = PrepareForLike(startsWith);
				const auto queryText = QString(d.startsWith).arg(GetRemovedFlag()).arg(escape);
				const auto query = db->CreateQuery(queryText.toStdString());
				query->Bind(":length", startsWith.length() + 1);
				query->Bind(":starts", startsWith.toStdString());
				query->Bind(":starts_like", startsWithLike.toStdString());
				for (query->Execute(); !query->Eof(); query->Next())
				{
					QString s = query->template Get<const char*>(0);
					const auto count = query->template Get<int>(1);
					if (s == startsWith)
					{
						equal.emplaceBack(std::move(s));
						equalCount += count;
					}
					else if (count == 1)
					{
						single.emplaceBack(std::move(s));
					}
					else
					{
						buffer.emplace(count, std::move(s));
					}
				}
			};

			selectStarts(startsWithGlobal);

			while (!buffer.empty() && equalCount + single.size() + static_cast<ptrdiff_t>(buffer.size()) < maxSize)
			{
				const auto startsWith = std::move(buffer.begin()->second);
				buffer.erase(buffer.begin());
				selectStarts(startsWith);
			}
		}

		auto head = GetHead(d.type, Loc::Tr(Loc::NAVIGATION, d.type), root, CreateSelf(root, d.type, parameters));

		for (auto&& [count, startsWith] : buffer)
		{
			Parameters p = parameters;
			const auto& s = (p["starts"] = std::move(startsWith));
			WriteEntry(head.children, root, d.type, p, s, QString("`%1`~").arg(s), QString::number(count));
		}

		Parameters typedParameters = parameters;
		typedParameters.erase("starts");

		for (const auto& s : single)
		{
			const auto [startsWithLike, escape] = PrepareForLike(s);
			const auto queryText = QString(d.selectSingle).arg(GetRemovedFlag()).arg(escape);
			const auto query = db->CreateQuery(queryText.toStdString());
			query->Bind(":starts", s.toStdString());
			query->Bind(":starts_like", startsWithLike.toStdString());
			for (query->Execute(); !query->Eof(); query->Next())
			{
				auto id = (typedParameters[d.type] = query->Get<const char*>(0));
				WriteEntry(head.children, root, "", typedParameters, std::move(id), query->Get<const char*>(1));
			}
		}

		for (const auto& s : equal)
		{
			const auto queryText = QString(d.selectEqual).arg(GetRemovedFlag());
			const auto query = db->CreateQuery(queryText.toStdString());
			query->Bind(":starts", s.toStdString());
			for (query->Execute(); !query->Eof(); query->Next())
			{
				auto id = (typedParameters[d.type] = query->Get<const char*>(0));
				WriteEntry(head.children, root, "", typedParameters, std::move(id), query->Get<const char*>(1));
			}
		}

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
