#pragma once
#include "StrUtil.h"

struct Book
{
	Book(size_t id_
		, std::wstring_view libId_
		, std::wstring_view title_
		, int seriesId_
		, int seriesNum_
		, std::wstring_view date_
		, int rate_
		, std::wstring_view language_
		, std::wstring_view folder_
		, std::wstring_view fileName_
		, size_t insideNo_
		, std::wstring_view format_
		, size_t size_
		, bool isDeleted_
		, std::wstring_view keywords_ = {}
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

	size_t      id;
	std::wstring libId;
	std::wstring title;
	int         seriesId;
	int         seriesNum;
	std::wstring date;
	int         rate;
	std::wstring language;
	std::wstring folder;
	std::wstring fileName;
	size_t      insideNo;
	std::wstring format;
	size_t      size;
	bool        isDeleted;
	std::wstring keywords;

private:
	static std::wstring InsertDot(std::wstring_view format)
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

	explicit Genre(std::wstring_view dbCode_)
		: dbCode(dbCode_)
	{
	}
	Genre(std::wstring_view code_, std::wstring_view parentCode_, std::wstring_view name_, size_t parentId_ = 0)
		: code(code_)
		, parentCore(parentCode_)
		, name(name_)
		, parentId(parentId_)
	{
	}
};

using Books = std::vector<Book>;
using Dictionary = std::map<std::wstring, size_t, StringLess<>>;
using Genres = std::vector<Genre>;
using Links = std::vector<std::pair<size_t, size_t>>;
using SettingsTableData = std::map<uint32_t, std::string>;

using GetIdFunctor = std::function<size_t(std::wstring_view)>;
using FindFunctor = std::function<Dictionary::const_iterator(const Dictionary &, std::wstring_view)>;

struct Data
{
	Books books;
	Dictionary authors, series;
	Genres genres;
	Links booksAuthors, booksGenres;
	SettingsTableData settings;
};

inline std::ostream & operator<<(std::ostream & stream, const Book & book)
{
	return stream << ToMultiByte(book.folder) << ", " << book.insideNo << ", " << ToMultiByte(book.libId) << ": " << book.id << ", " << ToMultiByte(book.title);
}

inline std::ostream & operator<<(std::ostream & stream, const Genre & genre)
{
	return stream << ToMultiByte(genre.dbCode) << ", " << ToMultiByte(genre.code) << ": " << ToMultiByte(genre.name);
}
