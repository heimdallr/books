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
struct Collection;

class IBooksListCreator // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Creator = IDataItem::Ptr (IBooksListCreator::*)() const;

public:
	virtual ~IBooksListCreator() = default;
	[[nodiscard]] virtual IDataItem::Ptr CreateReviewsList() const = 0;
	[[nodiscard]] virtual IDataItem::Ptr CreateGeneralList() const = 0;
};

class IBooksTreeCreator // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Creator = IDataItem::Ptr (IBooksTreeCreator::*)() const;

public:
	virtual ~IBooksTreeCreator() = default;
	[[nodiscard]] virtual IDataItem::Ptr CreateAuthorsTree() const = 0;
	[[nodiscard]] virtual IDataItem::Ptr CreateSeriesTree() const = 0;
	[[nodiscard]] virtual IDataItem::Ptr CreateReviewsTree() const = 0;
	[[nodiscard]] virtual IDataItem::Ptr CreateGeneralTree() const = 0;
};

struct QueryDescription;

class IBooksRootGenerator // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Generator = IDataItem::Ptr (IBooksRootGenerator::*)(const QueryDescription&) const;

public:
	virtual ~IBooksRootGenerator() = default;
	virtual IDataItem::Ptr GetList(const QueryDescription&) const = 0;
	virtual IDataItem::Ptr GetTree(const QueryDescription&) const = 0;
};

class IBookSelector // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Selector = void (IBookSelector::*)(const Collection& activeCollection, DB::IDatabase& db, const QueryDescription&);

public:
	virtual ~IBookSelector() = default;
	virtual void SelectBooks(const Collection& activeCollection, DB::IDatabase& db, const QueryDescription&) = 0;
	virtual void SelectReviews(const Collection& activeCollection, DB::IDatabase& db, const QueryDescription&) = 0;
};

using QueryDataExtractor = IDataItem::Ptr (*)(const DB::IQuery& query);

struct QueryDescription
{
	using Binder = int (*)(DB::IQuery&, const QString&);
	using MappingGetter = const BookItem::Mapping& (QueryDescription::*)() const noexcept;

	const char* navigationQuery { nullptr };
	QueryDataExtractor navigationExtractor { nullptr };
	const char* joinClause { nullptr };
	IBooksListCreator::Creator listCreator { nullptr };
	IBooksTreeCreator::Creator treeCreator { nullptr };
	BookItem::Mapping listMapping;
	BookItem::Mapping treeMapping;
	IBookSelector::Selector bookSelector { &IBookSelector::SelectBooks };

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
	, IBooksListCreator
	, IBooksTreeCreator
{
	NON_COPY_MOVABLE(BooksTreeGenerator)

public:
	BooksTreeGenerator(const Collection& activeCollection, DB::IDatabase& db, enum class NavigationMode navigationMode, QString navigationId, const QueryDescription& description);
	~BooksTreeGenerator() override;

public:
	NavigationMode GetNavigationMode() const noexcept;
	const QString& GetNavigationId() const noexcept;
	enum class ViewMode GetBooksViewMode() const noexcept;
	IDataItem::Ptr GetCached() const noexcept;
	void SetBooksViewMode(ViewMode viewMode) noexcept;
	BookInfo GetBookInfo(long long id) const;

private: // IBooksRootGenerator
	[[nodiscard]] IDataItem::Ptr GetList(const QueryDescription&) const override;
	[[nodiscard]] IDataItem::Ptr GetTree(const QueryDescription&) const override;

private: // IBooksListCreator
	[[nodiscard]] IDataItem::Ptr CreateReviewsList() const override;
	[[nodiscard]] IDataItem::Ptr CreateGeneralList() const override;

private: // IBooksTreeCreator
	[[nodiscard]] IDataItem::Ptr CreateAuthorsTree() const override;
	[[nodiscard]] IDataItem::Ptr CreateSeriesTree() const override;
	[[nodiscard]] IDataItem::Ptr CreateReviewsTree() const override;
	[[nodiscard]] IDataItem::Ptr CreateGeneralTree() const override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
