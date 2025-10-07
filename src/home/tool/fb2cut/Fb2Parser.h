#pragma once

#include <QString>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

class QIODevice;

namespace HomeCompa::fb2cut
{

struct Fb2EncodingParser
{
static QString GetEncoding(QIODevice& input);
};

class Fb2ImageParser
{
	NON_COPY_MOVABLE(Fb2ImageParser)

public:
	using OnBinaryFound = std::function<void(QString&&, bool isCover, const QByteArray& data)>;

	static bool Parse(QIODevice& input, OnBinaryFound binaryCallback);

private:
	Fb2ImageParser(QIODevice& input, OnBinaryFound binaryCallback);
	~Fb2ImageParser();

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

class Fb2Parser
{
	NON_COPY_MOVABLE(Fb2Parser)

public:
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

public:
	static void Parse(QString fileName, QIODevice& input, QIODevice& output, const std::unordered_map<QString, int>& replaceId);

private:
	Fb2Parser(QString fileName, QIODevice& input, QIODevice& output, const std::unordered_map<QString, int>& replaceId);
	~Fb2Parser();

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::fb2cut
