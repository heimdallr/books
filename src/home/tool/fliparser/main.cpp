﻿#include <QCryptographicHash>

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

#include <config/version.h>
#include <plog/Appenders/ConsoleAppender.h>

#include "fnd/ScopedCall.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "database/factory/Factory.h"
#include "inpx/src/util/constant.h"
#include "inpx/src/util/types.h"
#include "logging/LogAppender.h"
#include "logging/init.h"
#include "util/Fb2InpxParser.h"
#include "util/LogConsoleFormatter.h"
#include "util/executor/ThreadPool.h"
#include "util/xml/Initializer.h"

#include "Constant.h"
#include "log.h"
#include "zip.h"

using namespace HomeCompa;

namespace
{

using InpData = std::unordered_map<QString, std::vector<QByteArray>, CaseInsensitiveHash<QString>>;

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
	erase_if(strings, [](const QString& item) { return item.simplified().isEmpty(); });
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

	const auto tr = db.CreateTransaction();
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
		line = std::regex_replace(line, escapeBack, "$1");
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
with Books(BookId, Title, FileSize, LibID, Deleted, FileType, Time, Lang, keywords, LibRate) as (
    select b.BookId, b.Title, b.FileSize, b.BookId, b.Deleted, b.FileType, b.Time, b.Lang, b.keywords, avg(r.Rate)
        from libbook b
        left join librate r on r.BookID = b.BookId
        group by b.BookId
)
select distinct
    (select group_concat(
            case when m.rowid is null 
                then a.LastName ||','|| a.FirstName ||','|| a.MiddleName
                else m.LastName ||','|| m.FirstName ||','|| m.MiddleName
            end, ':')||':'
        from libavtorname a 
        join libavtor l on l.AvtorID = a.AvtorID and l.BookID = b.BookID
        left join libavtorname m on m.AvtorID = a.MasterId
        order by l.pos
    ) Author,
    (select group_concat(g.GenreCode, ':')||':'
        from libgenrelist g 
        join libgenre l on l.GenreId = g.GenreId and l.BookID = b.BookID 
        order by g.GenreID
    ) Genre,
    b.Title, s.SeqName, case when s.SeqId is null then null else ls.SeqNumb end, f.FileName, b.FileSize, b.LibID, b.Deleted, b.FileType, b.Time, b.Lang, b.LibRate, b.keywords
from Books b
left join libseq ls on ls.BookID = b.BookID
left join libseqname s on s.SeqID = ls.SeqID
left join libfilename f on f.BookId=b.BookID
)");
	
	size_t n = 0;
	for (query->Execute(); !query->Eof(); query->Next())
	{
		const std::string libId = query->Get<const char*>(7);
		const auto libRateValue = query->Get<double>(12);
		const auto libRate = libRateValue > 0.4 ? QString::number(std::llround(libRateValue)).toUtf8() : QByteArray {};

		const auto* modified = query->Get<const char*>(10);
		const auto* modifiedEnd = strchr(modified, ' ');

		std::string type = query->Get<const char*>(9);
		for (const auto* typoType : { "fd2", "fb", "???", "fb 2", "fbd" })
			if (type == typoType)
			{
				type = "fb2";
				break;
			}

		const auto* fileNamePtr = query->Get<const char*>(5);
		std::string fileName = fileNamePtr ? fileNamePtr : std::string {};

		const auto index = fileName.empty() ? libId + "." + type : fileName;
		if (fileName.empty())
		{
			fileName = libId;
		}
		else
		{
			QFileInfo fileInfo(QString::fromStdString(fileName));
			fileName = fileInfo.completeBaseName().toStdString();
			if (const auto ext = fileInfo.suffix().toLower(); ext == "fb2")
				type = "fb2";
		}

		QByteArray data;
		data.append(query->Get<const char*>(0)) // AUTHOR
			.append('\04')
			.append(query->Get<const char*>(1)) // GENRE
			.append('\04')
			.append(query->Get<const char*>(2)) // TITLE
			.append('\04')
			.append(query->Get<const char*>(3)) // SERIES
			.append('\04')
			.append(Util::Fb2InpxParser::GetSeqNumber(query->Get<const char*>(4)).toUtf8()) // SERNO
			.append('\04')
			.append(fileName.data()) // FILE
			.append('\04')
			.append(query->Get<const char*>(6)) // SIZE
			.append('\04')
			.append(libId) // LIBID
			.append('\04')
			.append(query->Get<const char*>(8)) // DEL
			.append('\04')
			.append(type.data()) // EXT
			.append('\04')
			.append(modified, modifiedEnd - modified) // DATE
			.append('\04')
			.append(query->Get<const char*>(11)) // LANG
			.append('\04')
			.append(libRate) // LIBRATE
			.append('\04')
			.append(query->Get<const char*>(13)) // KEYWORDS
			.append('\04');
		data.replace('\n', ' ');
		data.replace('\r', "");
		inpData[QString::fromStdString(index)].emplace_back(std::move(data));

		++n;
		PLOGV_IF(n % 50000 == 0) << n << " records selected";
	}

	PLOGV << n << " total records selected";
	return inpData;
}

std::unique_ptr<DB::IDatabase> CreateDatabase(const std::filesystem::path& sqlPath, const std::filesystem::path& archivesPath, const std::filesystem::path& outputFolder)
{
	const auto dbPath = outputFolder / (archivesPath.filename().wstring() + L".db");
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

	std::ranges::for_each(std::filesystem::directory_iterator { sqlPath },
	                      [&](const auto& entry)
	                      {
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
			tr->CreateCommand(index)->Execute();
		tr->Commit();
	}

	return db;
}

QByteArray CheckCustom(const Zip& zip, const QString& fileName, const QJsonObject& unIndexed)
{
	const auto fileData = zip.Read(fileName)->GetStream().readAll();
	QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);
	hash.addData(fileData);
	const auto key = hash.result().toHex();
	const auto it = unIndexed.constFind(key);
	if (it == unIndexed.constEnd())
		return {};

	QFileInfo fileInfo(fileName);

	const auto value = it.value().toObject();
	return QByteArray()
	    .append(value["author"].toString().toUtf8()) // AUTHOR
	    .append('\04')
	    .append(value["genre"].toString().toUtf8()) // GENRE
	    .append('\04')
	    .append(value["title"].toString().toUtf8()) // TITLE
	    .append('\04')
	    .append(value["series"].toString().toUtf8()) // SERIES
	    .append('\04')
	    .append(value["seqNumber"].toString().toUtf8()) // SERNO
	    .append('\04')
	    .append(fileInfo.baseName().toUtf8()) // FILE
	    .append('\04')
	    .append(QString::number(fileData.size()).toUtf8()) // SIZE
	    .append('\04')
	    .append(fileInfo.baseName().toUtf8()) // LIBID
	    .append('\04')
	    .append("1") // DEL
	    .append('\04')
	    .append(fileInfo.suffix().toUtf8()) // EXT
	    .append('\04')
	    .append(value["date"].toString().toUtf8()) // DATE
	    .append('\04')
	    .append(value["lang"].toString().toUtf8()) // LANG
	    .append('\04')
	    .append("") // LIBRATE
	    .append('\04')
	    .append(value["keywords"].toString().toUtf8()) // KEYWORDS
	    .append('\04');
}

QByteArray ParseBook(const QString& folder, const Zip& zip, const QString& fileName, const QDateTime& zipDateTime)
{
	return Util::Fb2InpxParser::Parse(folder, zip, fileName, zipDateTime, true).toUtf8();
}

std::unordered_set<long long> CreateInpx(DB::IDatabase& db, const InpData& inpData, const std::filesystem::path& archivesPath, const std::filesystem::path& outputFolder)
{
	std::unordered_set<long long> libIds;

	std::unordered_map<QString, long long, CaseInsensitiveHash<QString>> fileToId;
	{
		const auto query = db.CreateQuery("select FileName, BookId from libfilename");
		for (query->Execute(); !query->Eof(); query->Next())
			fileToId.emplace(query->Get<const char*>(0), query->Get<long long>(1));
	}

	const auto unIndexed = []() -> QJsonObject
	{
		QFile file(":/data/unindexed.json");
		[[maybe_unused]] const auto ok = file.open(QIODevice::ReadOnly);
		assert(ok);

		QJsonParseError jsonParserError;
		const auto doc = QJsonDocument::fromJson(file.readAll(), &jsonParserError);
		assert(jsonParserError.error == QJsonParseError::NoError && doc.isObject());

		return doc.object();
	}();

	const auto inpxFileName = outputFolder / (archivesPath.filename().wstring() + L".inpx");
	if (exists(inpxFileName))
		remove(inpxFileName);

	std::vector<QString> inpFiles;
	std::vector<QByteArray> data;

	std::ranges::transform(std::filesystem::directory_iterator { archivesPath }
	                           | std::views::filter([](const auto& entry) { return !entry.is_directory() && Zip::IsArchive(QString::fromStdWString(entry.path())); }),
	                       std::back_inserter(inpFiles),
	                       [&](const auto& entry)
	                       {
							   const auto& path = entry.path();
							   PLOGV << path.string();

							   QByteArray file;
							   const QFileInfo zipFileInfo(QString::fromStdWString(path));
							   Zip zip(zipFileInfo.filePath());
							   for (const auto& bookFile : zip.GetFileNameList())
							   {
								   if (const auto it = inpData.find(bookFile); it != inpData.end())
								   {
									   for (const auto& bytes : it->second)
										   file.append(bytes).append("\x0d\x0a");
								   }
								   else if (auto customBytes = CheckCustom(zip, bookFile, unIndexed); !customBytes.isEmpty())
								   {
									   file.append(customBytes).append("\x0d\x0a");
								   }
								   else if (auto parsedBytes = ParseBook(QString::fromStdWString(path.filename()), zip, bookFile, zipFileInfo.birthTime()); !parsedBytes.isEmpty())
								   {
									   file.append(parsedBytes).append("\x0d\x0a");
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
							   }

							   data.emplace_back(std::move(file));

							   return QString::fromStdWString(path.filename().replace_extension("inp"));
						   });

	if (const auto itFile = std::find_if(std::filesystem::directory_iterator { archivesPath },
	                                     std::filesystem::directory_iterator {},
	                                     [](const auto& entry) { return !entry.is_directory() && entry.path().extension() == ".inpx"; });
	    itFile != std::filesystem::directory_iterator {})
	{
		const auto& path = itFile->path();
		PLOGV << path.string();

		QByteArray file;
		Zip zip(QString::fromStdWString(path));
		std::ranges::transform(zip.GetFileNameList() | std::views::filter([](const auto& item) { return QFileInfo(item).suffix() != "inp"; }),
		                       std::back_inserter(data),
		                       [&](const auto& item)
		                       {
								   inpFiles.emplace_back(item);
								   return zip.Read(item)->GetStream().readAll();
							   });
	}

	Zip inpx(QString::fromStdWString(inpxFileName), ZipDetails::Format::Zip);

	const auto streamGetter = [&](const size_t n) -> std::unique_ptr<QIODevice>
	{
		PLOGV << inpFiles[n];
		return std::make_unique<QBuffer>(&data[n]);
	};

	const auto sizeGetter = [&](const size_t n) -> size_t { return data[n].size(); };

	inpx.Write(inpFiles, streamGetter, sizeGetter);

	return libIds;
}

std::vector<std::tuple<QString, QByteArray>> CreateReviewData(DB::IDatabase& db, const std::unordered_set<long long>& libIds, const std::filesystem::path& outputFolder)
{
	auto threadPool = std::make_unique<Util::ThreadPool>();

	const auto reviewsFolder = outputFolder / REVIEWS_FOLDER;
	[[maybe_unused]] const auto ok = create_directory(reviewsFolder);
	int currentMonth { -1 };
	std::map<long long, std::vector<std::tuple<QString, QString, QString>>> data;

	std::mutex archivesGuard;
	std::vector<std::tuple<QString, QByteArray>> archives;

	const auto write = [&](const int month)
	{
		ScopedCall monthGuard(
			[&]
			{
				currentMonth = month;
				data.clear();
			});
		if (currentMonth < 0)
			return;

		const auto archiveName = QString::fromStdWString(reviewsFolder / std::to_string(currentMonth)) + ".7z";
		if (const auto archivePath = std::filesystem::path(archiveName.toStdWString()); exists(archivePath))
			remove(archivePath);

		auto dataCopy = std::move(data);
		data = {};

		threadPool->enqueue(
			[&archivesGuard, &archives, archiveName = archiveName, data = std::move(dataCopy)]() mutable
			{
				const ScopedCall logGuard([&] { PLOGI << archiveName << " started"; }, [=] { PLOGI << archiveName << " finished"; });

				std::vector<QString> files;
				files.reserve(data.size());
				std::ranges::transform(data | std::views::keys, std::back_inserter(files), [](const long long id) { return QString::number(id); });

				std::vector<QByteArray> bytes;
				bytes.reserve(data.size());
				std::ranges::transform(data | std::views::values,
			                           std::back_inserter(bytes),
			                           [](auto& value)
			                           {
										   QJsonArray array;
										   for (auto& [name, time, text] : value)
										   {
											   text.prepend(' ');
											   text.append(' ');
											   array.append(QJsonObject {
												   { "name",              name.simplified() },
												   { "time",                           time },
												   { "text", ReplaceTags(text).simplified() },
											   });
										   }
										   return QJsonDocument(array).toJson();
									   });

				QByteArray zipBytes;
				{
					const auto streamGetter = [&](const size_t n) -> std::unique_ptr<QIODevice> { return std::make_unique<QBuffer>(&bytes[n]); };
					const auto sizeGetter = [&](const size_t n) -> size_t { return bytes[n].size(); };

					QBuffer buffer(&zipBytes);
					const ScopedCall bufferGuard([&] { buffer.open(QIODevice::WriteOnly); }, [&] { buffer.close(); });
					Zip zip(buffer, Zip::Format::SevenZip);
					zip.SetProperty(ZipDetails::PropertyId::SolidArchive, false);
					zip.SetProperty(Zip::PropertyId::CompressionMethod, QVariant::fromValue(Zip::CompressionMethod::Ppmd));
					zip.Write(files, streamGetter, sizeGetter);
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

void CreateReview(DB::IDatabase& db, const std::unordered_set<long long>& libIds, const std::filesystem::path& outputFolder)
{
	PLOGI << "write reviews";

	for (const auto& [fileName, data] : CreateReviewData(db, libIds, outputFolder))
	{
		QFile output(fileName);
		output.open(QIODevice::WriteOnly);
		output.write(data);
	}
}

std::vector<std::tuple<int, QByteArray, QByteArray>> CreateAuthorAnnotationsData(DB::IDatabase& db, const std::filesystem::path& sqlPath)
{
	auto threadPool = std::make_unique<Util::ThreadPool>();

	int currentId = -1;
	using PictureList = std::set<QString>;
	std::map<QString, std::pair<QString, PictureList>> data;

	std::mutex archivesGuard;
	std::vector<std::tuple<int, QByteArray, QByteArray>> archives;

	std::mutex picsGuard, zipGuard;
	std::unique_ptr<Zip> pics;
	std::unordered_set<QString> picsFiles;
	if (const auto picsArchiveName = sqlPath / "lib.a.attached.zip"; exists(picsArchiveName))
	{
		pics = std::make_unique<Zip>(QString::fromStdWString(picsArchiveName));
		auto files = pics->GetFileNameList();
		std::unordered_set(std::make_move_iterator(std::begin(files)), std::make_move_iterator(std::end(files))).swap(picsFiles);
	}

	const auto write = [&](const int id)
	{
		ScopedCall idGuard(
			[&]
			{
				currentId = id;
				data.clear();
			});
		if (currentId < 0)
			return;

		auto dataCopy = std::move(data);
		data = {};

		threadPool->enqueue(
			[&archivesGuard, &archives, &picsGuard, &zipGuard, &pics, &picsFiles, currentId, data = std::move(dataCopy)]() mutable
			{
				const ScopedCall logGuard([=] { PLOGI << "Authors pack " << currentId << " started"; }, [=] { PLOGI << "Authors pack " << currentId << " finished"; });

				QByteArray annotation;

				{
					std::vector<QString> files;
					files.reserve(data.size());
					std::ranges::transform(data | std::views::keys, std::back_inserter(files), [](const auto& item) { return item; });

					std::vector<QByteArray> bytes;
					bytes.reserve(data.size());
					std::ranges::transform(data | std::views::values | std::views::keys,
				                           std::back_inserter(bytes),
				                           [](QString& value)
				                           {
											   value.prepend(' ');
											   value.append(' ');
											   return ReplaceTags(value).simplified().toUtf8();
										   });

					const auto streamGetter = [&](const size_t n) -> std::unique_ptr<QIODevice> { return std::make_unique<QBuffer>(&bytes[n]); };

					const auto sizeGetter = [&](const size_t n) -> size_t { return bytes[n].size(); };

					QBuffer buffer(&annotation);
					const ScopedCall bufferGuard([&] { buffer.open(QIODevice::WriteOnly); }, [&] { buffer.close(); });
					Zip zip(buffer, Zip::Format::SevenZip);
					zip.SetProperty(ZipDetails::PropertyId::SolidArchive, false);
					zip.SetProperty(Zip::PropertyId::CompressionMethod, QVariant::fromValue(Zip::CompressionMethod::Ppmd));
					zip.Write(files, streamGetter, sizeGetter);
				}

				QByteArray pictures;

				if (pics)
				{
					std::vector<QString> files;
					std::vector<QByteArray> bytes;

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
								files.emplace_back(QString("%1/%2").arg(dstFolder, fileSplit.back()));

								std::lock_guard lock(picsGuard);
								bytes.emplace_back(pics->Read(file)->GetStream().readAll());
							}
						}
					}

					const auto streamGetter = [&](const size_t n) -> std::unique_ptr<QIODevice> { return std::make_unique<QBuffer>(&bytes[n]); };

					const auto sizeGetter = [&](const size_t n) -> size_t { return bytes[n].size(); };

					QBuffer buffer(&pictures);
					const ScopedCall bufferGuard([&] { buffer.open(QIODevice::WriteOnly); }, [&] { buffer.close(); });

					std::lock_guard zipLock(zipGuard);
					Zip zip(buffer, Zip::Format::Zip);
					zip.Write(files, streamGetter, sizeGetter);
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
		auto authorHash = hash.result().toHex();
		auto& files = data.emplace(std::move(authorHash), std::make_pair(QString(query->Get<const char*>(2)), PictureList {})).first->second.second;
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
	Log::LoggingInitializer logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, APP_ID).toStdWString());
	plog::ConsoleAppender<Util::LogConsoleFormatter> consoleAppender;
	Log::LogAppender logConsoleAppender(&consoleAppender);
	Util::XMLPlatformInitializer xmlPlatformInitializer;

	if (argc < 4)
	{
		PLOGE << "\n" << "usage:" << "\n" << "fliparser.exe sql_path archives_path output_folder";
		return 1;
	}

	const auto db = CreateDatabase(argv[1], argv[2], argv[3]);
	const auto inpData = GenerateInpData(*db);
	const auto libIds = CreateInpx(*db, inpData, argv[2], argv[3]);
	CreateReview(*db, libIds, argv[3]);
	CreateAuthorAnnotations(*db, argv[1], argv[3]);

	return 0;
}
