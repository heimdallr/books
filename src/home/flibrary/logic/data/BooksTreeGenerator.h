#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "DataItem.h"

namespace HomeCompa::DB {
class IQuery;
class IDatabase;
}

namespace HomeCompa::Flibrary {

class IBooksTreeCreator // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Creator = DataItem::Ptr(IBooksTreeCreator::*)() const;

public:
	virtual ~IBooksTreeCreator() = default;
	[[nodiscard]] virtual DataItem::Ptr CreateAuthorsTree() const = 0;
	[[nodiscard]] virtual DataItem::Ptr CreateSeriesTree() const = 0;
	[[nodiscard]] virtual DataItem::Ptr CreateGeneralTree() const = 0;
};

class IBooksRootGenerator // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Generator = DataItem::Ptr(IBooksRootGenerator::*)(IBooksTreeCreator::Creator) const;

public:
	virtual ~IBooksRootGenerator() = default;
	virtual DataItem::Ptr GetList(IBooksTreeCreator::Creator creator) const = 0;
	virtual DataItem::Ptr GetTree(IBooksTreeCreator::Creator creator) const = 0;
};

using QueryDataExtractor = DataItem::Ptr(*)(const DB::IQuery & query, const int * index);

struct QueryInfo
{
	QueryDataExtractor extractor;
	const int * index;
};

struct QueryDescription
{
	using Binder = int(*)(DB::IQuery &, const QString &);
	using MappingGetter = const BookItem::Mapping & (QueryDescription::*)() const noexcept;

	const char * query;
	const QueryInfo & queryInfo;
	const char * whereClause;
	const char * joinClause;
	Binder binder;
	IBooksTreeCreator::Creator treeCreator;
	BookItem::Mapping listMapping;
	BookItem::Mapping treeMapping;

	constexpr const BookItem::Mapping & GetListMapping() const noexcept{ return listMapping; }
	constexpr const BookItem::Mapping & GetTreeMapping() const noexcept{ return treeMapping; }
};

struct QStringWrapper
{
	const QString & data;
	bool operator<(const QStringWrapper & rhs) const
	{
		return QString::compare(data, rhs.data, Qt::CaseInsensitive) < 0;
	}
};

class BooksTreeGenerator final
	: public IBooksRootGenerator
	, public IBooksTreeCreator
{
	NON_COPY_MOVABLE(BooksTreeGenerator)

public:
	BooksTreeGenerator(DB::IDatabase & db
		, enum class NavigationMode navigationMode
		, QString navigationId
		, const QueryDescription & description
	);
	~BooksTreeGenerator() override;

public:
	NavigationMode GetNavigationMode() const noexcept;
	const QString & GetNavigationId() const noexcept;
	enum class ViewMode GetBooksViewMode() const noexcept;
	DataItem::Ptr GetCached() const noexcept;
	void SetBooksViewMode(ViewMode viewMode) noexcept;

private: // IBooksRootGenerator
	[[nodiscard]] DataItem::Ptr GetList(Creator creator) const override;
	[[nodiscard]] DataItem::Ptr GetTree(Creator creator) const override;

private: // IBooksTreeCreator
	[[nodiscard]] DataItem::Ptr CreateAuthorsTree() const override;
	[[nodiscard]] DataItem::Ptr CreateSeriesTree () const override;
	[[nodiscard]] DataItem::Ptr CreateGeneralTree() const override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

DataItem::Ptr CreateSimpleListItem(const DB::IQuery & query, const int * index);
void AppendTitle(QString & title, const QString & str, const QString & delimiter = " ");

}
