#pragma once

#include <unordered_set>

#include "util/StrUtil.h"

struct Book
{
	Book(const size_t id_
		, const std::wstring_view libId_
		, const std::wstring_view title_
		, const int seriesId_
		, const int seriesNum_
		, const std::wstring_view date_
		, const int rate_
		, const std::wstring_view language_
		, const std::wstring_view folder_
		, const std::wstring_view fileName_
		, const size_t insideNo_
		, const std::wstring_view format_
		, const size_t size_
		, const bool isDeleted_
		, const std::wstring_view keywords_ = {}
	)
		: id(id_)
		, libId(libId_)
		, title(title_)
		, seriesId(seriesId_)
		, seriesNum(seriesNum_)
		, date(date_)
		, rate(rate_)
		, language(language_)
		, folder(folder_)
		, fileName(fileName_)
		, insideNo(insideNo_)
		, format(InsertDot(format_))
		, size(size_)
		, isDeleted(isDeleted_)
		, keywords(keywords_)
	{
		std::ranges::transform(language, std::begin(language), towlower);
	}

	size_t       id;
	std::wstring libId;
	std::wstring title;
	int          seriesId;
	int          seriesNum;
	std::wstring date;
	int          rate;
	std::wstring language;
	std::wstring folder;
	std::wstring fileName;
	size_t       insideNo;
	std::wstring format;
	size_t       size;
	bool         isDeleted;
	std::wstring keywords;

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
	size_t parentId{ 0 };
	std::wstring dbCode;

	size_t childrenCount{ 0 };
	bool newGenre { true };

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

struct WStringHash
{
	using is_transparent = void;
	[[nodiscard]] size_t operator()(const wchar_t * txt) const
	{
		return std::hash<std::wstring_view>{}(txt);
	}
	[[nodiscard]] size_t operator()(const std::wstring_view txt) const
	{
		return std::hash<std::wstring_view>{}(txt);
	}
	[[nodiscard]] size_t operator()(const std::wstring & txt) const
	{
		return std::hash<std::wstring>{}(txt);
	}
};

using Books = std::vector<Book>;
using Dictionary = std::unordered_map<std::wstring, size_t, WStringHash, std::equal_to<>>;
using Genres = std::vector<Genre>;
using Links = std::vector<std::pair<size_t, size_t>>;
using SettingsTableData = std::unordered_map<uint32_t, std::string>;
using Folders = std::unordered_set<std::wstring, WStringHash, std::equal_to<>>;

using GetIdFunctor = std::function<size_t(std::wstring_view)>;
using FindFunctor = std::function<Dictionary::const_iterator(const Dictionary &, std::wstring_view)>;

struct Data
{
	Books books;
	Dictionary authors, series;
	Genres genres;
	Links booksAuthors, booksGenres;
	SettingsTableData settings;
	Folders folders;
};

inline std::ostream & operator<<(std::ostream & stream, const Book & book)
{
	return stream << ToMultiByte(book.folder) << ", " << book.insideNo << ", " << ToMultiByte(book.libId) << ": " << book.id << ", " << ToMultiByte(book.title);
}

inline std::ostream & operator<<(std::ostream & stream, const Genre & genre)
{
	return stream << ToMultiByte(genre.dbCode) << ", " << ToMultiByte(genre.code) << ": " << ToMultiByte(genre.name);
}

struct BookBuf
{
	std::wstring_view authors;
	std::wstring_view genres;
	std::wstring_view title;
	std::wstring_view seriesName;
	std::wstring_view seriesNum;
	std::wstring_view fileName;
	std::wstring_view size;
	std::wstring_view libId;
	std::wstring_view del;
	std::wstring_view ext;
	std::wstring_view date;
	std::wstring_view lang;
	std::wstring_view rate;
	std::wstring_view keywords;
};