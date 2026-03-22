#pragma once

#include <map>
#include <set>

#include "fnd/algorithm.h"

#include "util/StrUtil.h"

namespace HomeCompa
{

//AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;RATE;KEYWORDS;YEAR;SOURCELIB;
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
	BOOK_BUF_FIELD_ITEM(FOLDER)     \
	BOOK_BUF_FIELD_ITEM(LANG)       \
	BOOK_BUF_FIELD_ITEM(LIBRATE)    \
	BOOK_BUF_FIELD_ITEM(KEYWORDS)   \
	BOOK_BUF_FIELD_ITEM(YEAR)       \
	BOOK_BUF_FIELD_ITEM(SOURCELIB)

struct Book
{
	size_t  id;
	QString data;
#define BOOK_BUF_FIELD_ITEM(NAME) QStringView NAME;
	BOOK_BUF_FIELD_ITEMS_XMACRO
#undef BOOK_BUF_FIELD_ITEM
	QString fileName;
	QString folder;
	size_t  updateId;
};

struct Genre
{
	QString code;
	QString parentCore;
	QString name;
	size_t  parentId { 0 };
	QString dbCode;

	size_t childrenCount { 0 };
	bool   newGenre { true };

	explicit Genre(QString dbCode_)
		: dbCode { std::move(dbCode_) }
	{
	}

	Genre(QString code_, QString parentCode_, QString name_, const size_t parentId_ = 0)
		: code { std::move(code_) }
		, parentCore { std::move(parentCode_) }
		, name { std::move(name_) }
		, parentId { parentId_ }
	{
	}
};

struct Update
{
	size_t                          id { 0 };
	int                             title { 0 };
	size_t                          parentId { 0 };
	std::unordered_map<int, Update> children {};
};

class Dictionary
{
public:
	std::pair<QStringView, size_t> emplace(QString key, const size_t value)
	{
		if (const auto it = m_view.find(key); it != m_view.end())
			return std::make_pair(it->first, it->second);

		const auto it = m_data.try_emplace(std::move(key), value).first;
		return std::make_pair(it->first, it->second);
	}

	std::pair<QStringView, size_t> emplace(const QStringView key, const size_t value)
	{
		if (const auto it = m_data.find(key); it != m_data.end())
			return std::make_pair(it->first, it->second);

		const auto it = m_view.try_emplace(key, value).first;
		return std::make_pair(it->first, it->second);
	}

	bool contains(const QStringView key) const
	{
		return m_view.contains(key) || m_data.contains(key);
	}

	std::optional<size_t> find(const QStringView key) const
	{
		if (const auto it = m_view.find(key); it != m_view.end())
			return it->second;

		if (const auto it = m_data.find(key); it != m_data.end())
			return it->second;

		return std::nullopt;
	}

	size_t size() const noexcept
	{
		return m_data.size() + m_view.size();
	}

	template <typename F>
	std::optional<size_t> find_if(const F& f) const
	{
		if (const auto it = std::ranges::find_if(m_data, f); it != m_data.end())
			return it->second;

		if (const auto it = std::ranges::find_if(m_view, f); it != m_view.end())
			return it->second;

		return std::nullopt;
	}

private:
	std::unordered_map<QString, size_t, Util::WStringHash, Util::WStringEqualTo>     m_data;
	std::unordered_map<QStringView, size_t, Util::WStringHash, Util::WStringEqualTo> m_view;
};

using Books  = std::vector<Book>;
using Genres = std::vector<Genre>;
using Links  = std::unordered_map<size_t, std::vector<size_t>>;

using GetIdFunctor = std::function<size_t(QStringView)>;
using ParseChecker = std::function<bool(QStringView)>;
using Splitter     = std::function<std::vector<QString>(QStringView)>;
using InpxFolders  = std::map<std::pair<QString, QString>, QString, Util::CaseInsensitiveComparer<>>;
using BooksSeries  = std::unordered_map<size_t, std::vector<std::pair<size_t, std::optional<int>>>>;
using Reviews      = std::map<QString, std::set<QString>>;
using Annotations  = std::vector<std::pair<size_t, QString>>;
using FindFunctor  = std::function<std::optional<size_t>(const Dictionary&, QStringView)>;

struct Data
{
	Books       books;
	Dictionary  authors, series, keywords, bookFolders;
	Genres      genres;
	Update      updates;
	Links       booksAuthors, booksGenres, booksKeywords;
	InpxFolders inpxFolders;
	BooksSeries booksSeries;
	Reviews     reviews;
	Annotations annotations;
};

inline std::ostream& operator<<(std::ostream& stream, const Book& book)
{
	return stream << book.FOLDER.toString().toStdString() << ", " << book.LIBID.toString().toStdString() << ": " << book.id << ", " << book.TITLE.toString().toStdString();
}

inline std::ostream& operator<<(std::ostream& stream, const Genre& genre)
{
	return stream << genre.dbCode.toStdString() << ", " << genre.code.toStdString() << ": " << genre.name.toStdString();
}

} // namespace HomeCompa
