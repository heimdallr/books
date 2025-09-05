#pragma once

#include <map>
#include <set>

#include "util/StrUtil.h"

template <typename T>
QString ToQString(const T& str) = delete;

template <>
inline QString ToQString<std::string>(const std::string& str)
{
	return QString::fromStdString(str);
}

template <>
inline QString ToQString<QString>(const QString& str)
{
	return str;
}

template <>
inline QString ToQString<std::wstring>(const std::wstring& str)
{
	return QString::fromStdWString(str);
}

template <>
inline QString ToQString<std::pair<std::wstring, std::wstring>>(const std::pair<std::wstring, std::wstring>& str)
{
	return QString("%1/%2").arg(QString::fromStdWString(str.first), QString::fromStdWString(str.second));
}

template <>
inline QString ToQString<std::filesystem::path>(const std::filesystem::path& str)
{
	return QString::fromStdWString(str);
}

template <class T>
[[nodiscard]] T FakeCopyInit(T) noexcept = delete;

template <class T = void>
struct CaseInsensitiveComparer
{
	[[nodiscard]] constexpr bool operator()(const T& lhs, const T& rhs) const noexcept(noexcept(FakeCopyInit<bool>(lhs < rhs)))
	{
		return QString::compare(ToQString(lhs), ToQString(rhs), Qt::CaseInsensitive) < 0;
	}
};

template <>
struct CaseInsensitiveComparer<void>
{
	template <class L, class R>
	[[nodiscard]] constexpr auto operator()(L&& lhs, R&& rhs) const noexcept(noexcept(static_cast<L&&>(lhs) < static_cast<R&&>(rhs))) -> decltype(static_cast<L&&>(lhs) < static_cast<R&&>(rhs))
	{
		return QString::compare(ToQString(static_cast<L&&>(lhs)), ToQString(static_cast<R&&>(rhs)), Qt::CaseInsensitive) < 0;
	}

	using is_transparent = int;
};

template <typename T>
struct CaseInsensitiveHash
{
	size_t operator()(const T& value) const
	{
		return std::hash<QString>()(ToQString(value));
	}
};

template <typename First, typename Second>
struct PairHash
{
	size_t operator()(const std::pair<First, Second>& value) const
	{
		return std::rotl(std::hash<First>()(value.first), 1) | std::hash<Second>()(value.second);
	}
};

struct Book
{
	Book(const size_t id_,
	     const std::wstring_view libId_,
	     const std::wstring_view title_,
	     const int seriesId_,
	     const int seriesNum_,
	     const std::wstring_view date_,
	     const int rate_,
	     const std::wstring_view language_,
	     const size_t folder_,
	     const std::wstring_view fileName_,
	     const size_t insideNo_,
	     const std::wstring_view format_,
	     const size_t size_,
	     const bool deleted_,
	     const size_t updateId_,
	     const int year_)
		: id { id_ }
		, libId { libId_ }
		, title { title_ }
		, seriesId { seriesId_ }
		, seriesNum { seriesNum_ }
		, date { date_ }
		, rate { rate_ }
		, language { language_ }
		, folder { folder_ }
		, fileName { fileName_ }
		, insideNo { insideNo_ }
		, format { InsertDot(format_) }
		, size { size_ }
		, deleted { deleted_ }
		, updateId { updateId_ }
		, year { year_ }
	{
		std::ranges::transform(language, std::begin(language), towlower);
	}

	size_t id;
	std::wstring libId;
	std::wstring title;
	int seriesId;
	int seriesNum;
	std::wstring date;
	int rate;
	std::wstring language;
	size_t folder;
	std::wstring fileName;
	size_t insideNo;
	std::wstring format;
	size_t size;
	bool deleted;
	size_t updateId;
	int year;

private:
	static std::wstring InsertDot(const std::wstring_view format)
	{
		std::wstring result(L".");
		result.insert(result.end(), std::cbegin(format), std::cend(format));
		return result;
	}
};

struct Genre
{
	std::wstring code;
	std::wstring parentCore;
	std::wstring name;
	size_t parentId { 0 };
	std::wstring dbCode;

	size_t childrenCount { 0 };
	bool newGenre { true };
	bool dateGenre { false };

	explicit Genre(const std::wstring_view dbCode_)
		: dbCode(dbCode_)
	{
	}

	Genre(const std::wstring_view code_, const std::wstring_view parentCode_, const std::wstring_view name_, const size_t parentId_ = 0)
		: code(code_)
		, parentCore(parentCode_)
		, name(name_)
		, parentId(parentId_)
	{
	}
};

struct Update
{
	size_t id { 0 };
	int title { 0 };
	size_t parentId { 0 };
	std::unordered_map<int, Update> children {};
};

struct WStringHash
{
	using is_transparent = void;

	[[nodiscard]] size_t operator()(const wchar_t* txt) const
	{
		return std::hash<std::wstring_view> {}(txt);
	}

	[[nodiscard]] size_t operator()(const std::wstring_view txt) const
	{
		return std::hash<std::wstring_view> {}(txt);
	}

	[[nodiscard]] size_t operator()(const std::wstring& txt) const
	{
		return std::hash<std::wstring> {}(txt);
	}
};

using Books = std::vector<Book>;
using Dictionary = std::unordered_map<std::wstring, size_t, WStringHash, std::equal_to<>>;
using Genres = std::vector<Genre>;
using Links = std::unordered_map<size_t, std::vector<size_t>>;
using Folders = std::unordered_map<std::wstring, size_t, CaseInsensitiveHash<std::wstring>>;

using GetIdFunctor = std::function<size_t(std::wstring_view)>;
using FindFunctor = std::function<Dictionary::const_iterator(const Dictionary&, std::wstring_view)>;
using ParseChecker = std::function<bool(std::wstring_view)>;
using Splitter = std::function<std::vector<std::wstring>(std::wstring_view)>;
using InpxFolders = std::map<std::pair<std::wstring, std::wstring>, std::string, CaseInsensitiveComparer<>>;
using BooksSeries = std::unordered_map<size_t, std::vector<std::pair<size_t, std::optional<int>>>>;
using Reviews = std::map<size_t, std::set<std::string>>;

struct Data
{
	Books books;
	Dictionary authors, series, keywords;
	Genres genres;
	Update updates;
	Links booksAuthors, booksGenres, booksKeywords;
	Folders bookFolders;
	InpxFolders inpxFolders;
	BooksSeries booksSeries;
	Reviews reviews;
};

inline std::ostream& operator<<(std::ostream& stream, const Book& book)
{
	return stream << book.folder << ", " << book.insideNo << ", " << ToMultiByte(book.libId) << ": " << book.id << ", " << ToMultiByte(book.title);
}

inline std::ostream& operator<<(std::ostream& stream, const Genre& genre)
{
	return stream << ToMultiByte(genre.dbCode) << ", " << ToMultiByte(genre.code) << ": " << ToMultiByte(genre.name);
}

//AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;RATE;KEYWORDS;
#define BOOK_BUF_FIELD_ITEMS_XMACRO \
	BOOK_BUF_FIELD_ITEM(AUTHOR)     \
	BOOK_BUF_FIELD_ITEM(GENRE)      \
	BOOK_BUF_FIELD_ITEM(TITLE)      \
	BOOK_BUF_FIELD_ITEM(SERIES)     \
	BOOK_BUF_FIELD_ITEM(SERNO)      \
	BOOK_BUF_FIELD_ITEM(FILE)       \
	BOOK_BUF_FIELD_ITEM(SIZE)       \
	BOOK_BUF_FIELD_ITEM(LIBID)      \
	BOOK_BUF_FIELD_ITEM(DEL)        \
	BOOK_BUF_FIELD_ITEM(EXT)        \
	BOOK_BUF_FIELD_ITEM(DATE)       \
	BOOK_BUF_FIELD_ITEM(INSNO)      \
	BOOK_BUF_FIELD_ITEM(FOLDER)     \
	BOOK_BUF_FIELD_ITEM(LANG)       \
	BOOK_BUF_FIELD_ITEM(LIBRATE)    \
	BOOK_BUF_FIELD_ITEM(KEYWORDS)   \
	BOOK_BUF_FIELD_ITEM(YEAR)

struct BookBuf
{
#define BOOK_BUF_FIELD_ITEM(NAME) std::wstring_view NAME;
	BOOK_BUF_FIELD_ITEMS_XMACRO
#undef BOOK_BUF_FIELD_ITEM
};
