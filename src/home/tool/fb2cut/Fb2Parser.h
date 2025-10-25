#pragma once

#include <QString>
#include <QStringList>

class QIODevice;

namespace HomeCompa::fb2cut
{

struct Fb2EncodingParser
{
	static QString GetEncoding(QIODevice& input);
};

struct Fb2ImageParser
{
	using OnBinaryFound = std::function<void(QString&&, bool isCover, const QByteArray& data)>;
	static bool Parse(QIODevice& input, OnBinaryFound binaryCallback);
};

struct Fb2Parser
{
	static constexpr const char* FB2_TAGS[] {
		"p",
		"fictionbook",
		"description",
		"body",
		"binary",
		"title-info",
		"src-title-info",
		"document-info",
		"publish-info",
		"custom-info",
		"genre",
		"author",
		"first-name",
		"last-name",
		"middle-name",
		"nickname",
		"nick-name",
		"nick",
		"email",
		"home-page",
		"book-title",
		"annotation",
		"poem",
		"cite",
		"subtitle",
		"empty-line",
		"keywords",
		"date",
		"coverpage",
		"image",
		"lang",
		"src-lang",
		"translator",
		"sequence",
		"program-used",
		"src-url",
		"src-ocr",
		"id",
		"version",
		"history",
		"book-author",
		"book-name",
		"publisher",
		"city",
		"year",
		"isbn",
		"title",
		"epigraph",
		"section",
		"v",
		"a",
		"text-author",
		"strong",
		"emphasis",
		"sub",
		"sup",
		"strikethrough",
		"code",
		"stanza",
		"i",
		"b",
		"u",
		"table",
		"tr",
		"td",
		"th",
		"img",
		"pre",
		"style",
		"center",
		"h1",
		"h2",
		"h3",
		"h4",
		"h5",
		"br",
		"stylesheet",
		"username",
		"em",
		"ol",
		"li",
		"col",
	};

	struct ParseResult
	{
		QString     title;
		QString     hashText;
		QStringList hashSections;
	};

	static ParseResult Parse(QString fileName, QIODevice& input, QIODevice& output, const std::unordered_map<QString, int>& replaceId);
};

struct HashParser
{
#define HASH_PARSER_CALLBACK_ITEMS_X_MACRO \
	HASH_PARSER_CALLBACK_ITEM(id)          \
	HASH_PARSER_CALLBACK_ITEM(folder)      \
	HASH_PARSER_CALLBACK_ITEM(file)        \
	HASH_PARSER_CALLBACK_ITEM(title)

	using Callback = std::function<void(
#define HASH_PARSER_CALLBACK_ITEM(NAME) QString NAME,
		HASH_PARSER_CALLBACK_ITEMS_X_MACRO
#undef HASH_PARSER_CALLBACK_ITEM
			QString cover,
		QStringList images
	)>;
	static void Parse(QIODevice& input, Callback callback);
};

} // namespace HomeCompa::fb2cut
