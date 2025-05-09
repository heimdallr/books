#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "DataItem.h"

namespace HomeCompa::DB
{
class IQuery;
class IDatabase;
}

namespace HomeCompa::Flibrary
{

class IBooksTreeCreator // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Creator = IDataItem::Ptr (IBooksTreeCreator::*)() const;

public:
	virtual ~IBooksTreeCreator() = default;
	[[nodiscard]] virtual IDataItem::Ptr CreateAuthorsTree() const = 0;
	[[nodiscard]] virtual IDataItem::Ptr CreateSeriesTree() const = 0;
	[[nodiscard]] virtual IDataItem::Ptr CreateGeneralTree() const = 0;
};

class IBooksRootGenerator // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Generator = IDataItem::Ptr (IBooksRootGenerator::*)(IBooksTreeCreator::Creator) const;

public:
	virtual ~IBooksRootGenerator() = default;
	virtual IDataItem::Ptr GetList(IBooksTreeCreator::Creator creator) const = 0;
	virtual IDataItem::Ptr GetTree(IBooksTreeCreator::Creator creator) const = 0;
};

using QueryDataExtractor = IDataItem::Ptr (*)(const DB::IQuery& query, const size_t* index, size_t removedIndex);

struct QueryInfo
{
	QueryDataExtractor extractor;
	const size_t* index;
	size_t removedIndex;
};

struct QueryDescription
{
	using Binder = int (*)(DB::IQuery&, const QString&);
	using MappingGetter = const BookItem::Mapping& (QueryDescription::*)() const noexcept;

	const char* query;
	const QueryInfo& queryInfo;
	const char* whereClause;
	const char* joinClause;
	Binder binder;
	IBooksTreeCreator::Creator treeCreator;
	BookItem::Mapping listMapping;
	BookItem::Mapping treeMapping;
	const char* seqNumberTableAlias { "b" };
	const char* with { nullptr };

	constexpr const BookItem::Mapping& GetListMapping() const noexcept
	{
		return listMapping;
	}

	constexpr const BookItem::Mapping& GetTreeMapping() const noexcept
	{
		return treeMapping;
	}
};

class BooksTreeGenerator final
	: public IBooksRootGenerator
	, public IBooksTreeCreator
{
	NON_COPY_MOVABLE(BooksTreeGenerator)

public:
	BooksTreeGenerator(DB::IDatabase& db, enum class NavigationMode navigationMode, QString navigationId, const QueryDescription& description);
	~BooksTreeGenerator() override;

public:
	NavigationMode GetNavigationMode() const noexcept;
	const QString& GetNavigationId() const noexcept;
	enum class ViewMode GetBooksViewMode() const noexcept;
	IDataItem::Ptr GetCached() const noexcept;
	void SetBooksViewMode(ViewMode viewMode) noexcept;
	BookInfo GetBookInfo(long long id) const;

private: // IBooksRootGenerator
	[[nodiscard]] IDataItem::Ptr GetList(Creator creator) const override;
	[[nodiscard]] IDataItem::Ptr GetTree(Creator creator) const override;

private: // IBooksTreeCreator
	[[nodiscard]] IDataItem::Ptr CreateAuthorsTree() const override;
	[[nodiscard]] IDataItem::Ptr CreateSeriesTree() const override;
	[[nodiscard]] IDataItem::Ptr CreateGeneralTree() const override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
