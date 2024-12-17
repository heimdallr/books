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
	using Creator = IDataItem::Ptr(IBooksTreeCreator::*)() const;

public:
	virtual ~IBooksTreeCreator() = default;
	[[nodiscard]] virtual IDataItem::Ptr CreateAuthorsTree() const = 0;
	[[nodiscard]] virtual IDataItem::Ptr CreateSeriesTree() const = 0;
	[[nodiscard]] virtual IDataItem::Ptr CreateGeneralTree() const = 0;
};

class IBooksRootGenerator // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Generator = IDataItem::Ptr(IBooksRootGenerator::*)(IBooksTreeCreator::Creator) const;

public:
	virtual ~IBooksRootGenerator() = default;
	virtual IDataItem::Ptr GetList(IBooksTreeCreator::Creator creator) const = 0;
	virtual IDataItem::Ptr GetTree(IBooksTreeCreator::Creator creator) const = 0;
};

using QueryDataExtractor = IDataItem::Ptr(*)(const DB::IQuery & query, const int * index);

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
		if (data.isEmpty() || rhs.data.isEmpty())
			return !rhs.data.isEmpty();

		const auto lCategory = Category(data[0]), rCategory = Category(rhs.data[0]);
		return lCategory != rCategory ? lCategory < rCategory : QString::compare(data, rhs.data, Qt::CaseInsensitive) < 0;
	}

private:
	[[nodiscard]] int Category(const QChar c) const noexcept
	{
		assert(c.category() < static_cast<int>(std::size(m_categories)));
		if (const auto result = m_categories[c.category()]; result != 0)
			return result;

		return c.row() == 4 ? 0 : 1;
	}

private:
	static constexpr int m_categories[]
	{
0,	//		Mark_NonSpacing,          //   Mn
0,	//		Mark_SpacingCombining,    //   Mc
0,	//		Mark_Enclosing,           //   Me
//
2,	//		Number_DecimalDigit,      //   Nd
0,	//		Number_Letter,            //   Nl
0,	//		Number_Other,             //   No
//
0,	//		Separator_Space,          //   Zs
0,	//		Separator_Line,           //   Zl
0,	//		Separator_Paragraph,      //   Zp
//
8,	//		Other_Control,            //   Cc
8,	//		Other_Format,             //   Cf
8,	//		Other_Surrogate,          //   Cs
8,	//		Other_PrivateUse,         //   Co
8,	//		Other_NotAssigned,        //   Cn
//
0,	//		Letter_Uppercase,         //   Lu
0,	//		Letter_Lowercase,         //   Ll
0,	//		Letter_Titlecase,         //   Lt
0,	//		Letter_Modifier,          //   Lm
0,	//		Letter_Other,             //   Lo
//
6,	//		Punctuation_Connector,    //   Pc
6,	//		Punctuation_Dash,         //   Pd
6,	//		Punctuation_Open,         //   Ps
6,	//		Punctuation_Close,        //   Pe
6,	//		Punctuation_InitialQuote, //   Pi
6,	//		Punctuation_FinalQuote,   //   Pf
6,	//		Punctuation_Other,        //   Po
//
4,	//		Symbol_Math,              //   Sm
4,	//		Symbol_Currency,          //   Sc
4,	//		Symbol_Modifier,          //   Sk
4,	//		Symbol_Other              //   So
	};
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
	IDataItem::Ptr GetCached() const noexcept;
	void SetBooksViewMode(ViewMode viewMode) noexcept;
	BookInfo GetBookInfo(long long id) const;

private: // IBooksRootGenerator
	[[nodiscard]] IDataItem::Ptr GetList(Creator creator) const override;
	[[nodiscard]] IDataItem::Ptr GetTree(Creator creator) const override;

private: // IBooksTreeCreator
	[[nodiscard]] IDataItem::Ptr CreateAuthorsTree() const override;
	[[nodiscard]] IDataItem::Ptr CreateSeriesTree () const override;
	[[nodiscard]] IDataItem::Ptr CreateGeneralTree() const override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
