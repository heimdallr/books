#include <QCryptographicHash>

#include <filesystem>
#include <fstream>
#include <regex>

#include <QBuffer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

#include <plog/Appenders/ConsoleAppender.h>

#include "fnd/ScopedCall.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/ProductConstant.h"

#include "database/factory/Factory.h"
#include "inpx/constant.h"
#include "inpx/inpx.h"
#include "inpx/types.h"
#include "logging/LogAppender.h"
#include "logging/init.h"
#include "util/Fb2InpxParser.h"
#include "util/LogConsoleFormatter.h"
#include "util/executor/ThreadPool.h"
#include "util/xml/Initializer.h"

#include "Constant.h"
#include "log.h"
#include "zip.h"

#include "config/version.h"

using namespace HomeCompa;

namespace
{

struct Series
{
	QString title;
	QString serNo;
	int     type;
	int     level;
};

struct Book
{
	QString             author;
	QString             genre;
	QString             title;
	std::vector<Series> series;
	QString             file;
	QString             size;
	QString             libId;
	bool                deleted;
	QString             ext;
	QString             date;
	QString             lang;
	double              rate;
	QString             keywords;
	QString             year;

	static Book fromString(const QString& str)
	{
		if (str.isEmpty())
			return {};

		//"AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;LIBRATE;KEYWORDS;YEAR;"
		auto l = str.split('\04');
		assert(l.size() == 15);
		return Book {
			.author   = std::move(l[0]),
			.genre    = std::move(l[1]),
			.title    = std::move(l[2]),
			.series   = { { std::move(l[3]), std::move(l[4]) } },
			.file     = std::move(l[5]),
			.size     = std::move(l[6]),
			.libId    = std::move(l[7]),
			.deleted  = l[8] == "1",
			.ext      = std::move(l[9]),
			.date     = std::move(l[10]),
			.lang     = QString::fromStdWString(Inpx::Parser::GetLang(l[11].toLower().toStdWString())),
			.rate     = l[12].toDouble(),
			.keywords = std::move(l[13]),
			.year     = std::move(l[14]),
		};
	}
};

QByteArray& operator<<(QByteArray& bytes, const Book& book)
{
	const auto rate      = std::llround(book.rate);
	const auto rateBytes = rate > 0 && rate <= 5 ? QString::number(rate).toUtf8() : QByteArray {};

	for (const auto& [seriesTitle, serNo, type, level] : book.series)
	{
		QByteArray data;
		data.append(book.author.toUtf8())
			.append('\04')
			.append(book.genre.toUtf8())
			.append('\04')
			.append(book.title.toUtf8())
			.append('\04')
			.append(seriesTitle.toUtf8())
			.append('\04')
			.append(serNo.toUtf8())
			.append('\04')
			.append(book.file.toUtf8())
			.append('\04')
			.append(book.size.toUtf8())
			.append('\04')
			.append(book.libId.toUtf8())
			.append('\04')
			.append(book.deleted ? "1" : "0")
			.append('\04')
			.append(book.ext.toUtf8())
			.append('\04')
			.append(book.date.toUtf8())
			.append('\04')
			.append(book.lang.toUtf8())
			.append('\04')
			.append(rateBytes)
			.append('\04')
			.append(book.keywords.toUtf8())
			.append('\04')
			.append(book.year.toUtf8())
			.append('\04');
		data.replace('\n', ' ');
		data.replace('\r', "");
		data.append("\x0d\x0a");

		bytes.append(data);
	}
	return bytes;
}

using InpData      = std::unordered_map<QString, Book, CaseInsensitiveHash<QString>>;
using FileToFolder = std::unordered_map<QString, QStringList>;

constexpr auto APP_ID = "fliparser";

constexpr auto g_libaannotations = R"(CREATE TABLE libaannotations (
  AvtorId INTEGER,
  nid INTEGER,
  Title VARCHAR(255),
  Body TEXT
))";

constexpr auto g_libapics = R"(CREATE TABLE libapics (
  AvtorId INTEGER,
  nid INTEGER,
  File VARCHAR(255)
))";

constexpr auto g_libbannotations = R"(CREATE TABLE libbannotations (
  BookId INTEGER,
  nid INTEGER,
  Title VARCHAR(255),
  Body TEXT
))";

constexpr auto g_libbpics = R"(CREATE TABLE libbpics (
  BookId INTEGER,
  nid INTEGER,
  File VARCHAR(255)
))";

constexpr auto g_libavtor = R"(CREATE TABLE libavtor (
  BookId INTEGER,
  AvtorId INTEGER,
  Pos INTEGER
))";

constexpr auto g_libavtorname = R"(CREATE TABLE libavtorname (
  AvtorId INTEGER,
  FirstName VARCHAR(99),
  MiddleName VARCHAR(99),
  LastName VARCHAR(99),
  NickName VARCHAR(33),
  uid INTEGER,
  Email VARCHAR(255),
  Homepage VARCHAR(255),
  Gender CHAR,
  MasterId INTEGER
))";

constexpr auto g_libbook = R"(CREATE TABLE libbook (
  BookId INTEGER,
  FileSize INTEGER,
  Time DATETIME,
  Title VARCHAR(254),
  Title1 VARCHAR(254),
  Lang VARCHAR(3),
  LangEx INTEGER,
  SrcLang VARCHAR(3),
  FileType VARCHAR(4),
  Encoding VARCHAR(32),
  Year INTEGER,
  Deleted VARCHAR(1),
  Ver VARCHAR(8),
  FileAuthor VARCHAR(64),
  N INTEGER,
  keywords VARCHAR(255),
  md5 VARCHAR(32),
  Modified DATETIME,
  pmd5 VARCHAR(32),
  InfoCode INTEGER,
  Pages INTEGER,
  Chars INTEGER
))";

constexpr auto g_libfilename = R"(CREATE TABLE libfilename (
  BookId INTEGER,
  FileName VARCHAR(255)
))";

constexpr auto g_libgenre = R"(CREATE TABLE libgenre (
  Id INTEGER,
  BookId INTEGER,
  GenreId INTEGER
))";

constexpr auto g_libgenrelist = R"(CREATE TABLE libgenrelist (
  GenreId INTEGER,
  GenreCode VARCHAR(45),
  GenreDesc VARCHAR(99),
  GenreMeta VARCHAR(45)
))";

constexpr auto g_libjoinedbooks = R"(CREATE TABLE libjoinedbooks (
  Id INTEGER,
  Time DATETIME,
  BadId INTEGER,
  GoodId INTEGER,
  realId INTEGER
))";

constexpr auto g_librate = R"(CREATE TABLE librate (
  ID INTEGER,
  BookId INTEGER,
  UserId INTEGER,
  Rate CHAR
))";

constexpr auto g_librecs = R"(CREATE TABLE librecs (
  id INTEGER,
  uid INTEGER,
  bid INTEGER,
  timestamp DATETIME
))";

constexpr auto g_libseq = R"(CREATE TABLE libseq (
  BookId INTEGER,
  SeqId INTEGER,
  SeqNumb INTEGER,
  Level INTEGER,
  Type INTEGER
))";

constexpr auto g_libseqname = R"(CREATE TABLE libseqname (
  SeqId INTEGER,
  SeqName VARCHAR(254)
))";

constexpr auto g_libtranslator = R"(CREATE TABLE libtranslator (
  BookId INTEGER,
  TranslatorId INTEGER,
  Pos INTEGER
))";

constexpr auto g_libreviews = R"(CREATE TABLE libreviews (
  Name VARCHAR(255),
  Time DATETIME,
  BookId INTEGER,
  Text TEXT
))";

constexpr const char* g_commands[] { g_libaannotations, g_libapics,       g_libbannotations, g_libbpics, g_libavtor, g_libavtorname, g_libbook,       g_libfilename, g_libgenre,
	                                 g_libgenrelist,    g_libjoinedbooks, g_librate,         g_librecs,  g_libseq,   g_libseqname,   g_libtranslator, g_libreviews };

constexpr const char* g_indices[] {
	"CREATE INDEX ix_libavtor_BookID_Pos ON libavtor (BookId, Pos)",
	"CREATE INDEX ix_libavtor_AvtorID ON libavtor (AvtorId)",
	"CREATE INDEX ix_libavtorname_primary_key ON libavtorname (AvtorId)",
	"CREATE INDEX ix_libbook_primary_key ON libbook (BookId)",
	"CREATE INDEX ix_libfilename_primary_key ON libfilename (BookId)",
	"CREATE INDEX ix_libgenre_BookID ON libgenre (BookId)",
	"CREATE INDEX ix_libgenre_GenreID ON libgenre (GenreId)",
	"CREATE INDEX ix_libgenrelist_primary_key ON libgenrelist (GenreId)",
	"CREATE INDEX ix_librate_BookID ON librate (BookId)",
	"CREATE INDEX ix_libseq_BookID ON libseq (BookId)",
	"CREATE INDEX ix_libseq_SeqID ON libseq (SeqId)",
	"CREATE INDEX ix_libseqname_primary_key ON libseqname (SeqId)",
	"CREATE INDEX ix_libreviews_Time ON libreviews (Time)",
	"CREATE INDEX ix_libaannotations_nid ON libaannotations (nid)",
	"CREATE INDEX ix_libapics_AvtorId ON libapics (AvtorId)",
	"delete from libseq where not exists(select 42 from libseqname where libseqname.SeqId = libseq.SeqId)",
};

void ReplaceStringInPlace(std::string& subject, const std::string& search, const std::string& replace)
{
	size_t pos = 0;
	while ((pos = subject.find(search, pos)) != std::string::npos)
	{
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}
}

QString& ReplaceTags(QString& str)
{
	static constexpr std::pair<const char*, const char*> tags[] {
		{    "br",    "br" },
        {    "hr",    "hr" },
        { "quote",     "q" },
        { "table", "table" },
        {    "tr",    "tr" },
        {    "th",    "th" },
        {    "td",    "td" },
	};

	str.replace("<p>&nbsp;</p>", "");

	auto strings = str.split('\n', Qt::SkipEmptyParts);
	erase_if(strings, [](const QString& item) {
		return item.simplified().isEmpty();
	});
	str = strings.join("<br/>");

	str.replace(QRegularExpression(R"(\[(\w)\])"), R"(<\1>)").replace(QRegularExpression(R"(\[(/\w)\])"), R"(<\1>)");
	for (const auto& [from, to] : tags)
		str.replace(QString("[%1]").arg(from), QString("<%1>").arg(to), Qt::CaseInsensitive).replace(QString("[/%1]").arg(from), QString("</%1>").arg(to), Qt::CaseInsensitive);

	str.replace(QRegularExpression(R"(\[img\](.*?)\[/img\])"), R"(<img src="\1"/>)");
	str.replace(QRegularExpression(R"(\[(URL|url)=(.*?)\](.*?)\[/(URL|url)\])"), R"(<a href="\2"/>\3</a>)");
	str.replace(QRegularExpression(R"(\[color=(.*?)\])"), R"(<font color="\1">)").replace("[/color]", "</font>");

	str.replace(QRegularExpression(R"(([^"])(https{0,1}:\/\/\S+?)([\s<]))"), R"(\1<a href="\2">\2</a>\3)");

	str.replace(QRegularExpression(R"(\[collapse collapsed title=(.*?)\])"), R"(<details><summary>\1</summary>)");
	str.replace(QRegularExpression(R"(\[/collapse])"), R"(</details>)");

	return str;
}

void FillTables(DB::IDatabase& db, const std::filesystem::path& path)
{
	LOGI << path.string();

	std::ifstream inp(path);
	inp.seekg(0, std::ios_base::end);
	const auto size = inp.tellg();
	inp.seekg(0, std::ios_base::beg);

	const auto  tr = db.CreateTransaction();
	std::string line;

	const std::regex escape(R"(\\(.))"), escapeBack("\x04(.)\x05");

	int64_t currentPercents = 0;
	while (std::getline(inp, line))
	{
		if (!line.starts_with("INSERT INTO"))
			continue;

		ReplaceStringInPlace(line, R"(\\\")", "\"");
		ReplaceStringInPlace(line, R"(\r\n)", "\n");
		ReplaceStringInPlace(line, R"(\\n)", "\n");
		ReplaceStringInPlace(line, R"(\n)", "\n");
		line = std::regex_replace(line, escape, "\x04$1\x05");
		ReplaceStringInPlace(line, "\x04'\x05", "''");
		line                           = std::regex_replace(line, escapeBack, "$1");
		[[maybe_unused]] const auto ok = tr->CreateCommand(line)->Execute();
		assert(ok);
		if (const auto percents = 100 * inp.tellg() / size; percents != currentPercents)
			LOGI << path.stem().string() << " " << (currentPercents = percents) << "%";
	}

	LOGI << path.stem().string() << " " << 100 << "%";

	tr->Commit();
}

// AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;LIBRATE;KEYWORDS;
InpData GenerateInpData(DB::IDatabase& db)
{
	InpData inpData;

	const auto query = db.CreateQuery(R"(
with Books(  BookId,   Title,   FileSize,   LibID,    Deleted,   FileType,   Time,   Lang,   Keywords, Year,              LibRate) as (
    select b.BookId, b.Title, b.FileSize, b.BookId, b.Deleted, b.FileType, b.Time, b.Lang, b.keywords, nullif(b.Year, 0), avg(r.Rate)
        from libbook b
        left join librate r on r.BookID = b.BookId
        group by b.BookId
)
select
    (select group_concat(
            case when m.rowid is null 
                then n.LastName ||','|| n.FirstName ||','|| n.MiddleName
                else m.LastName ||','|| m.FirstName ||','|| m.MiddleName
            end, ':')||':'
		from libavtor l
		join libavtorname n on n.AvtorId = l.AvtorId
		left join libavtorname m on m.AvtorID = n.MasterId
		where l.BookId = b.BookID 
			and (n.NickName != 'иллюстратор' or not exists (
				select 42 
				from libavtor ll
				join libavtorname nn on nn.AvtorId = ll.AvtorId and nn.NickName != 'иллюстратор'
				where ll.BookId = l.BookId )
			)
		order by l.Pos
    ) Author,
    (select group_concat(g.GenreCode, ':')||':'
        from libgenrelist g 
        join libgenre l on l.GenreId = g.GenreId and l.BookID = b.BookID 
        order by g.GenreID
    ) Genre,
    b.Title, s.SeqName, case when s.SeqId is null then null else ls.SeqNumb end, f.FileName, b.FileSize, b.LibID, b.Deleted, b.FileType, b.Time, b.Lang, b.LibRate, b.keywords, b.Year, ls.Type, ls.Level
from Books b
left join libseq ls on ls.BookID = b.BookID
left join libseqname s on s.SeqID = ls.SeqID
left join libfilename f on f.BookId=b.BookID
)");

	PLOGV << "records selection started";

	size_t n = 0;
	for (query->Execute(); !query->Eof(); query->Next())
	{
		QString libId = query->Get<const char*>(7);

		QString type = query->Get<const char*>(9);
		if (type != "fb2")
			for (const auto* typoType : { "fd2", "fb", "???", "fb 2", "fbd" })
				if (type == typoType)
				{
					type = "fb2";
					break;
				}

		QString fileName = query->Get<const char*>(5);

		auto index = fileName.isEmpty() ? libId + "." + type : fileName;

		auto it = inpData.find(index);
		if (it == inpData.end())
		{
			if (fileName.isEmpty())
			{
				fileName = libId;
			}
			else
			{
				QFileInfo fileInfo(fileName);
				fileName = fileInfo.completeBaseName();
				if (const auto ext = fileInfo.suffix().toLower(); ext == "fb2")
					type = "fb2";
			}

			const auto* deleted = query->Get<const char*>(8);

			it = inpData
			         .try_emplace(
						 std::move(index),
						 Book {
							 .author   = query->Get<const char*>(0),
							 .genre    = query->Get<const char*>(1),
							 .title    = query->Get<const char*>(2),
							 .file     = std::move(fileName),
							 .size     = query->Get<const char*>(6),
							 .libId    = std::move(libId),
							 .deleted  = deleted && *deleted != '0',
							 .ext      = std::move(type),
							 .date     = QString::fromUtf8(query->Get<const char*>(10), 10),
							 .lang     = QString::fromStdWString(Inpx::Parser::GetLang(QString(query->Get<const char*>(11)).toLower().toStdWString())),
							 .rate     = query->Get<double>(12),
							 .keywords = query->Get<const char*>(13),
							 .year     = query->Get<const char*>(14),
						 }
					 )
			         .first;
		}

		it->second.series.emplace_back(query->Get<const char*>(3), Util::Fb2InpxParser::GetSeqNumber(query->Get<const char*>(4)), query->Get<int>(15), query->Get<int>(16));

		++n;
		PLOGV_IF(n % 50000 == 0) << n << " records selected";
	}

	PLOGV << n << " total records selected";

	for (auto& [_, book] : inpData)
		std::ranges::sort(book.series, {}, [](const Series& item) {
			return std::tuple(item.type, -item.level);
		});

	return inpData;
}

std::unique_ptr<DB::IDatabase> CreateDatabase(const std::filesystem::path& sqlPath, const std::filesystem::path& archivesPath, const std::filesystem::path& outputFolder)
{
	const auto dbPath   = outputFolder / (archivesPath.filename().wstring() + L".db");
	const auto dbExists = exists(dbPath);

	auto db = Create(DB::Factory::Impl::Sqlite, std::format("path={};flag={}", dbPath.string(), dbExists ? "READONLY" : "CREATE"));
	if (dbExists)
		return db;

	{
		const auto tr = db->CreateTransaction();
		for (const auto* command : g_commands)
			tr->CreateCommand(command)->Execute();
		tr->Commit();
	}

	std::ranges::for_each(std::filesystem::directory_iterator { sqlPath }, [&](const auto& entry) {
		if (entry.is_directory())
			return;

		auto path = entry.path();
		if (path.extension() != ".sql")
			return;

		FillTables(*db, path.make_preferred());
	});
	{
		const auto tr = db->CreateTransaction();
		for (const auto* index : g_indices)
		{
			PLOGI << index;
			tr->CreateCommand(index)->Execute();
		}
		tr->Commit();
	}

	return db;
}

Book CheckCustom(const Zip& zip, const QString& fileName, const QJsonObject& unIndexed)
{
	const auto         fileData = zip.Read(fileName)->GetStream().readAll();
	QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);
	hash.addData(fileData);
	const auto key = hash.result().toHex();
	const auto it  = unIndexed.constFind(key);
	if (it == unIndexed.constEnd())
		return {};

	QFileInfo fileInfo(fileName);

	const auto value = it.value().toObject();

	std::vector<Series> series;
	if (const auto seriesObj = value["series"]; seriesObj.isArray())
		std::ranges::transform(seriesObj.toArray(), std::back_inserter(series), [](const auto& item) {
			const auto obj = item.toObject();
			return Series { obj["title"].toString(), obj["number"].toString() };
		});
	if (series.empty())
		series.emplace_back();

	return Book {
		.author   = value["author"].toString(),
		.genre    = value["genre"].toString(),
		.title    = value["title"].toString(),
		.series   = std::move(series),
		.file     = fileInfo.baseName(),
		.size     = QString::number(fileData.size()),
		.libId    = fileInfo.baseName(),
		.deleted  = true,
		.ext      = fileInfo.suffix(),
		.date     = value["date"].toString(),
		.lang     = value["lang"].toString(),
		.keywords = value["keywords"].toString(),
		.year     = value["year"].toString(),
	};
}

Book ParseBook(const QString& folder, const Zip& zip, const QString& fileName, const QDateTime& zipDateTime)
{
	return Book::fromString(Util::Fb2InpxParser::Parse(folder, zip, fileName, zipDateTime, true));
}

std::unordered_set<long long> CreateInpx(DB::IDatabase& db, InpData& inpData, const std::filesystem::path& archivesPath, const std::filesystem::path& outputFolder, FileToFolder& fileToFolder)
{
	std::unordered_set<long long> libIds;

	std::unordered_map<QString, long long, CaseInsensitiveHash<QString>> fileToId;
	{
		const auto query = db.CreateQuery("select FileName, BookId from libfilename");
		for (query->Execute(); !query->Eof(); query->Next())
			fileToId.try_emplace(query->Get<const char*>(0), query->Get<long long>(1));
	}

	const auto unIndexed = []() -> QJsonObject {
		QFile                       file(":/data/unindexed.json");
		[[maybe_unused]] const auto ok = file.open(QIODevice::ReadOnly);
		assert(ok);

		QJsonParseError jsonParserError;
		const auto      doc = QJsonDocument::fromJson(file.readAll(), &jsonParserError);
		assert(jsonParserError.error == QJsonParseError::NoError && doc.isObject());

		return doc.object();
	}();

	const auto inpxFileName = outputFolder / (archivesPath.filename().wstring() + L".inpx");
	if (exists(inpxFileName))
		remove(inpxFileName);

	auto      zipFileController = Zip::CreateZipFileController();
	QDateTime maxTime;

	std::ranges::for_each(
		std::filesystem::directory_iterator { archivesPath } | std::views::filter([](const auto& entry) {
			return !entry.is_directory() && Zip::IsArchive(QString::fromStdWString(entry.path()));
		}),
		[&](const auto& entry) {
			const auto& path = entry.path();
			PLOGV << path.string();

			QByteArray      file;
			const QFileInfo zipFileInfo(QString::fromStdWString(path));
			Zip             zip(zipFileInfo.filePath());
			for (const auto& bookFile : zip.GetFileNameList())
			{
				if (const auto it = inpData.find(bookFile); it != inpData.end())
				{
					file << it->second;
				}
				else if (auto customBook = CheckCustom(zip, bookFile, unIndexed); !customBook.title.isEmpty())
				{
					file << customBook;
					inpData[bookFile] = std::move(customBook);
				}
				else if (auto parsedBook = ParseBook(QString::fromStdWString(path.filename()), zip, bookFile, zipFileInfo.birthTime()); !parsedBook.title.isEmpty())
				{
					file << parsedBook;
					inpData[bookFile] = std::move(parsedBook);
				}
				else
				{
					PLOGW << zipFileInfo.filePath() << "/" << bookFile << " not found";
					continue;
				}

				if (const auto it = fileToId.find(bookFile); it != fileToId.end())
				{
					libIds.insert(it->second);
				}
				else
				{
					bool ok = false;
					if (const auto libId = QFileInfo(bookFile).baseName().toLongLong(&ok); ok)
						libIds.insert(libId);
				}

				maxTime = std::max(maxTime, zip.GetFileTime(bookFile));
				fileToFolder[bookFile].emplace_back(zipFileInfo.fileName());
			}

			if (!file.isEmpty())
				zipFileController->AddFile(QString::fromStdWString(path.filename().replace_extension("inp")), std::move(file), QDateTime::currentDateTime());
		}
	);

	if (const auto itFile = std::find_if(
			std::filesystem::directory_iterator { archivesPath },
			std::filesystem::directory_iterator {},
			[](const auto& entry) {
				return !entry.is_directory() && entry.path().extension() == ".inpx";
			}
		);
	    itFile != std::filesystem::directory_iterator {})
	{
		const auto& path = itFile->path();
		PLOGV << path.string();

		Zip zip(QString::fromStdWString(path));
		std::ranges::for_each(
			zip.GetFileNameList() | std::views::filter([](const auto& item) {
				return QFileInfo(item).suffix() != "inp" && item != STRUCTURE_INFO;
			}),
			[&](const auto& item) {
				zipFileController->AddFile(
					item,
					[&] {
						return item == "version.info" ? maxTime.toString("yyyyMMdd").toUtf8() : zip.Read(item)->GetStream().readAll();
					}(),
					QDateTime::currentDateTime()
				);
			}
		);
		zipFileController->AddFile(STRUCTURE_INFO, "AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;LIBRATE;KEYWORDS;YEAR;", QDateTime::currentDateTime());
	}

	{
		Zip inpx(QString::fromStdWString(inpxFileName), ZipDetails::Format::Zip);
		inpx.Write(std::move(zipFileController));
	}

	return libIds;
}

QByteArray CreateReviewAdditional(const InpData& inpData, const std::unordered_set<long long>& libIds)
{
	QJsonObject jsonObject;
	for (const auto& book : inpData | std::views::values)
	{
		if (book.rate <= std::numeric_limits<double>::epsilon() || !libIds.contains(book.libId.toLongLong()))
			continue;

		jsonObject.insert(
			book.libId,
			QJsonObject {
				{ "libRate", book.rate }
        }
		);
	}

	if (jsonObject.isEmpty())
		return {};

	return QJsonDocument(jsonObject).toJson();
}

std::vector<std::tuple<QString, QByteArray>> CreateReviewData(DB::IDatabase& db, const InpData& inpData, const std::unordered_set<long long>& libIds, const std::filesystem::path& outputFolder)
{
	auto threadPool = std::make_unique<Util::ThreadPool>();

	const auto                                                              reviewsFolder = outputFolder / REVIEWS_FOLDER;
	[[maybe_unused]] const auto                                             ok            = create_directory(reviewsFolder);
	int                                                                     currentMonth { -1 };
	std::map<long long, std::vector<std::tuple<QString, QString, QString>>> data;

	std::mutex                                   archivesGuard;
	std::vector<std::tuple<QString, QByteArray>> archives;

	threadPool->enqueue([&]() {
		auto             archiveName = QString::fromStdWString(reviewsFolder / REVIEWS_ADDITIONAL_ARCHIVE_NAME);
		const ScopedCall logGuard(
			[&] {
				PLOGI << archiveName << " started";
			},
			[=] {
				PLOGI << archiveName << " finished";
			}
		);

		auto additional = CreateReviewAdditional(inpData, libIds);
		if (additional.isEmpty())
			return;

		QByteArray zipBytes;
		{
			QBuffer          buffer(&zipBytes);
			const ScopedCall bufferGuard(
				[&] {
					buffer.open(QIODevice::WriteOnly);
				},
				[&] {
					buffer.close();
				}
			);
			Zip  zip(buffer, Zip::Format::Zip);
			auto zipFiles = Zip::CreateZipFileController();
			zipFiles->AddFile(REVIEWS_ADDITIONAL_BOOKS_FILE_NAME, std::move(additional));
			zip.Write(std::move(zipFiles));
		}

		std::lock_guard lock(archivesGuard);
		archives.emplace_back(std::move(archiveName), std::move(zipBytes));
	});

	const auto write = [&](const int month) {
		ScopedCall monthGuard([&] {
			currentMonth = month;
			data.clear();
		});
		if (currentMonth < 0)
			return;

		auto archiveName = QString::fromStdWString(reviewsFolder / std::to_string(currentMonth)) + ".7z";

		auto dataCopy = std::move(data);
		data          = {};

		threadPool->enqueue([&archivesGuard, &archives, archiveName = std::move(archiveName), data = std::move(dataCopy)]() mutable {
			const ScopedCall logGuard(
				[&] {
					PLOGI << archiveName << " started";
				},
				[=] {
					PLOGI << archiveName << " finished";
				}
			);

			auto zipFiles = Zip::CreateZipFileController();
			std::ranges::for_each(data, [&](auto& value) {
				QJsonArray array;
				for (auto& [name, time, text] : value.second)
				{
					text.prepend(' ');
					text.append(' ');
					array.append(QJsonObject {
						{ Flibrary::Constant::NAME,              name.simplified() },
						{ Flibrary::Constant::TIME,                           time },
						{ Flibrary::Constant::TEXT, ReplaceTags(text).simplified() },
					});
				}
				zipFiles->AddFile(QString::number(value.first), QJsonDocument(array).toJson());
			});

			QByteArray zipBytes;
			{
				QBuffer          buffer(&zipBytes);
				const ScopedCall bufferGuard(
					[&] {
						buffer.open(QIODevice::WriteOnly);
					},
					[&] {
						buffer.close();
					}
				);
				Zip zip(buffer, Zip::Format::SevenZip);
				zip.SetProperty(ZipDetails::PropertyId::SolidArchive, false);
				zip.SetProperty(Zip::PropertyId::CompressionMethod, QVariant::fromValue(Zip::CompressionMethod::Ppmd));
				zip.Write(std::move(zipFiles));
			}

			std::lock_guard lock(archivesGuard);
			archives.emplace_back(std::move(archiveName), std::move(zipBytes));
		});
	};

	const auto query = db.CreateQuery("select r.BookId, r.Name, r.Time, r.Text from libreviews r order by r.Time");
	for (query->Execute(); !query->Eof(); query->Next())
	{
		const auto bookId = query->Get<long long>(0);
		if (!libIds.contains(bookId))
			continue;

		const auto* date = query->Get<const char*>(2);
		if (const auto month = atoi(date) * 100 + atoi(date + 5); month != currentMonth) // NOLINT(cert-err34-c)
			write(month);

		data[bookId].emplace_back(query->Get<const char*>(1), query->Get<const char*>(2), query->Get<const char*>(3));
	}

	write(currentMonth);

	threadPool.reset();

	return archives;
}

void CreateBookList(const InpData& inpData, const FileToFolder& fileToFolder, const std::filesystem::path& outputFolder)
{
	PLOGI << "write contents";

	const auto getSortedString = [](const QString& src) {
		return src.isEmpty() ? QString(QChar { 0xffff }) : src.toLower().simplified();
	};
	const auto getSortedNum = [](const QString& src) {
		if (src.isEmpty())
			return std::numeric_limits<int>::max();

		bool       ok    = false;
		const auto value = src.toInt(&ok);
		return ok ? value : std::numeric_limits<int>::max();
	};

	std::unordered_map<QString, std::vector<std::pair<const Book*, std::tuple<QString, QString, int, QString>>>> langs;
	for (const auto& book : inpData | std::views::values)
		if (fileToFolder.contains(book.file + "." + book.ext))
			langs[book.lang].emplace_back(
				&book,
				std::make_tuple(getSortedString(book.author), getSortedString(book.series.front().title), getSortedNum(book.series.front().serNo), getSortedString(book.title))
			);

	auto zipFiles = Zip::CreateZipFileController();
	std::ranges::for_each(langs, [&](auto& value) {
		QByteArray data;
		std::ranges::sort(value.second, {}, [](const auto& item) {
			return item.second;
		});

		for (const Book* book : value.second | std::views::keys)
		{
			const auto fileName = book->file + "." + book->ext;
			const auto it       = fileToFolder.find(fileName);
			if (it == fileToFolder.end())
				continue;

			data.append(book->author.toUtf8()).append("\t").append(book->title.toUtf8()).append("\t");
			if (!book->series.empty())
			{
				if (const auto& series = book->series.front(); !series.title.isEmpty())
				{
					data.append("[").append(series.title.toUtf8());
					if (!series.serNo.isEmpty())
						data.append(" #").append(series.serNo.toUtf8());
					data.append("]");
				}
			}
			data.append("\t").append(it->second.join(',').toUtf8()).append("\t").append(fileName.toUtf8()).append("\x0d\x0a");
		}

		zipFiles->AddFile(value.first + ".txt", std::move(data));
	});

	PLOGI << "archive contents";
	remove(outputFolder / "contents.7z");

	Zip zip(QString::fromStdWString(outputFolder / "contents.7z"), Zip::Format::SevenZip);
	zip.SetProperty(ZipDetails::PropertyId::SolidArchive, false);
	zip.SetProperty(Zip::PropertyId::CompressionMethod, QVariant::fromValue(Zip::CompressionMethod::Ppmd));
	zip.Write(std::move(zipFiles));
}

void CreateReview(DB::IDatabase& db, const InpData& inpData, const std::unordered_set<long long>& libIds, const std::filesystem::path& outputFolder)
{
	PLOGI << "write reviews";

	for (const auto& [fileName, data] : CreateReviewData(db, inpData, libIds, outputFolder))
	{
		QFile output(fileName);
		output.open(QIODevice::WriteOnly);
		output.write(data);
	}
}

std::vector<std::tuple<int, QByteArray, QByteArray>> CreateAuthorAnnotationsData(DB::IDatabase& db, const std::filesystem::path& sqlPath)
{
	auto threadPool = std::make_unique<Util::ThreadPool>();

	int currentId     = -1;
	using PictureList = std::set<QString>;
	std::map<QString, std::pair<QString, PictureList>> data;

	std::mutex                                           archivesGuard;
	std::vector<std::tuple<int, QByteArray, QByteArray>> archives;

	std::mutex                  picsGuard, zipGuard;
	std::unique_ptr<Zip>        pics;
	std::unordered_set<QString> picsFiles;
	if (const auto picsArchiveName = sqlPath / "lib.a.attached.zip"; exists(picsArchiveName))
	{
		pics       = std::make_unique<Zip>(QString::fromStdWString(picsArchiveName));
		auto files = pics->GetFileNameList();
		std::unordered_set(std::make_move_iterator(std::begin(files)), std::make_move_iterator(std::end(files))).swap(picsFiles);
	}

	const auto write = [&](const int id) {
		ScopedCall idGuard([&] {
			currentId = id;
			data.clear();
		});
		if (currentId < 0)
			return;

		auto dataCopy = std::move(data);
		data          = {};

		threadPool->enqueue([&archivesGuard, &archives, &picsGuard, &zipGuard, &pics, &picsFiles, currentId, data = std::move(dataCopy)]() mutable {
			const ScopedCall logGuard(
				[=] {
					PLOGI << "Authors pack " << currentId << " started";
				},
				[=] {
					PLOGI << "Authors pack " << currentId << " finished";
				}
			);

			QByteArray annotation;

			{
				auto zipFiles = Zip::CreateZipFileController();
				std::ranges::for_each(data, [&](auto& value) {
					value.second.first.prepend(' ');
					value.second.first.append(' ');
					zipFiles->AddFile(value.first, ReplaceTags(value.second.first).simplified().toUtf8());
				});

				QBuffer          buffer(&annotation);
				const ScopedCall bufferGuard(
					[&] {
						buffer.open(QIODevice::WriteOnly);
					},
					[&] {
						buffer.close();
					}
				);
				Zip zip(buffer, Zip::Format::SevenZip);
				zip.SetProperty(ZipDetails::PropertyId::SolidArchive, false);
				zip.SetProperty(Zip::PropertyId::CompressionMethod, QVariant::fromValue(Zip::CompressionMethod::Ppmd));
				zip.Write(std::move(zipFiles));
			}

			QByteArray pictures;

			if (pics)
			{
				auto zipFiles = Zip::CreateZipFileController();
				for (const auto& [dstFolder, values] : data)
				{
					std::unordered_set<QString> uniqueFiles;
					for (const auto& file : values.second)
					{
						if (!picsFiles.contains(file))
							continue;

						const auto fileSplit = file.split('/', Qt::SkipEmptyParts);
						if (fileSplit.size() != 3)
							continue;

						if (uniqueFiles.insert(fileSplit.back()).second)
						{
							std::lock_guard lock(picsGuard);
							auto            picBody = pics->Read(file)->GetStream().readAll();
							if (picBody.isEmpty())
								PLOGW << fileSplit.join("/") << " is empty";
							else
								zipFiles->AddFile(QString("%1/%2").arg(dstFolder, fileSplit.back()), std::move(picBody), pics->GetFileTime(file));
						}
					}
				}

				QBuffer          buffer(&pictures);
				const ScopedCall bufferGuard(
					[&] {
						buffer.open(QIODevice::WriteOnly);
					},
					[&] {
						buffer.close();
					}
				);

				std::lock_guard zipLock(zipGuard);
				Zip             zip(buffer, Zip::Format::Zip);
				zip.Write(std::move(zipFiles));
			}

			std::lock_guard lock(archivesGuard);
			archives.emplace_back(currentId, std::move(annotation), std::move(pictures));
		});
	};

	const auto query = db.CreateQuery(R"(
select 
    n.nid / 10000, a.LastName || ' ' || a.FirstName || ' ' || a.MiddleName, n.Body, p.File
from libaannotations n 
join libavtorname a on a.AvtorId = n.AvtorId 
left join libapics p on p.AvtorId = n.AvtorId
order by n.nid
)");
	for (query->Execute(); !query->Eof(); query->Next())
	{
		if (const auto id = query->Get<int>(0); id != currentId)
			write(id);

		QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);
		hash.addData(QString(query->Get<const char*>(1)).split(' ', Qt::SkipEmptyParts).join(' ').toLower().simplified().toUtf8());
		auto& files = data.try_emplace(hash.result().toHex(), std::make_pair(QString(query->Get<const char*>(2)), PictureList {})).first->second.second;
		if (const auto* file = query->Get<const char*>(3))
			files.insert(file);
	}

	write(currentId);
	threadPool.reset();

	return archives;
}

void CreateAuthorAnnotations(DB::IDatabase& db, const std::filesystem::path& sqlPath, const std::filesystem::path& outputFolder)
{
	PLOGI << "write author annotations";

	const auto authorsFolder = outputFolder / AUTHORS_FOLDER;
	create_directory(authorsFolder);

	const auto authorImagesFolder = authorsFolder / Global::PICTURES;
	create_directory(authorImagesFolder);

	for (const auto& [id, annotation, images] : CreateAuthorAnnotationsData(db, sqlPath))
	{
		if (!annotation.isEmpty())
		{
			const auto archiveName = QString::fromStdWString(authorsFolder / std::to_string(id)) + ".7z";
			if (const auto archivePath = std::filesystem::path(archiveName.toStdWString()); exists(archivePath))
				remove(archivePath);

			QFile output(archiveName);
			output.open(QIODevice::WriteOnly);
			output.write(annotation);
		}
		if (!images.isEmpty())
		{
			const auto archiveName = QString::fromStdWString(authorImagesFolder / std::to_string(id)) + ".zip";
			if (const auto archivePath = std::filesystem::path(archiveName.toStdWString()); exists(archivePath))
				remove(archivePath);

			QFile output(archiveName);
			output.open(QIODevice::WriteOnly);
			output.write(images);
		}
	}
}

} // namespace

int main(const int argc, char* argv[])
{
	Log::LoggingInitializer                          logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, APP_ID).toStdWString());
	plog::ConsoleAppender<Util::LogConsoleFormatter> consoleAppender;
	Log::LogAppender                                 logConsoleAppender(&consoleAppender);
	Util::XMLPlatformInitializer                     xmlPlatformInitializer;

	if (argc < 4)
	{
		PLOGE << "\n" << "usage:" << "\n" << "fliparser.exe sql_path archives_path output_folder";
		return 1;
	}

	const auto db      = CreateDatabase(argv[1], argv[2], argv[3]);
	auto       inpData = GenerateInpData(*db);

	FileToFolder fileToFolder;
	const auto   libIds = CreateInpx(*db, inpData, argv[2], argv[3], fileToFolder);
	CreateBookList(inpData, fileToFolder, argv[3]);

	CreateReview(*db, inpData, libIds, argv[3]);
	CreateAuthorAnnotations(*db, argv[1], argv[3]);

	return 0;
}
