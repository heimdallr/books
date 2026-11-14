#include "inpx/inpx.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "database/interface/IFactory.h"

#include "interface/logic/IDatabaseProvider.h"

#include "database/factory/Factory.h"
#include "inpx/InpxConstant.h"

#include "zip.h"

namespace HomeCompa::Flibrary::Test {

using SetInpx      = std::function<void(IZipFileController&)>;
using Check        = std::function<void(const Inpx::UpdateResult&, DB::IDatabase&)>;
using ParseProcess = void (Inpx::Parser::*)(Inpx::Parser::IniMap data, Inpx::CreateCollectionMode mode, Inpx::Parser::Callback callback);

auto Process(const SetInpx& setInpx, const Check& check, const ParseProcess process, const QString& archiveFolder = "folder", Inpx::CreateCollectionMode createMode = Inpx::CreateCollectionMode::None)
{
	static constexpr auto dbPath = "file:my_shared_mem_db?mode=memory&cache=shared";
	auto                  db     = DB::Factory::Create<DB::IDatabaseWritable>(DB::IFactory::Impl::Sqlite, std::format(R"(path="{}")", dbPath));

	QByteArray bytes;
	{
		const auto zipFiles = Zip::CreateZipFileController();
		setInpx(*zipFiles);

		QBuffer zipStream(&bytes);
		zipStream.open(QIODevice::WriteOnly);
		Zip zip(zipStream, Zip::Format::Zip);
		zip.SetProperty(Zip::PropertyId::CompressionMethod, QVariant::fromValue(Zip::CompressionMethod::Copy));
		zip.SetProperty(Zip::PropertyId::CompressionLevel, QVariant::fromValue(Zip::CompressionLevel::None));
		zip.Write(*zipFiles);
	}

	QEventLoop           eventLoop;
	Inpx::Parser         parser;
	Inpx::Parser::IniMap ini {
		{                        DB_PATH,                                           dbPath },
		{               DB_CREATE_SCRIPT,                   ":/data/CreateCollection.json" },
		{               DB_UPDATE_SCRIPT,                   ":/data/UpdateCollection.json" },
		{                    GENRES_PATH,                             ":/data/genres.json" },
		{ SET_DATABASE_VERSION_STATEMENT, IDatabaseProvider::GetDatabaseVersionStatement() },
		{                      INPX_PATH,  QString::fromUtf8(bytes.toBase64()) + "_BASE64" },
		{                 ARCHIVE_FOLDER,                                    archiveFolder },
	};

	std::invoke(process, std::ref(parser), std::move(ini), createMode, [&](const Inpx::UpdateResult& result) {
		check(result, *db);
		eventLoop.exit();
	});

	eventLoop.exec();

	return DB::Factory::Create<DB::IDatabase>(DB::IFactory::Impl::Sqlite, std::format(R"(path="{}")", dbPath));
}

struct InpxTestData
{
	struct Check
	{
		QString                               query;
		std::vector<std::vector<std::string>> results;
	};

	std::vector<QString> files;
	std::vector<Check>   checks;
	Inpx::UpdateResult   result { .genres = 1 };

	std::vector<QString> updateFiles;
	std::vector<Check>   updateChecks;
	Inpx::UpdateResult   updateResult { .genres = 1 };
};

struct InpxTest : testing::TestWithParam<InpxTestData>
{
};

INSTANTIATE_TEST_SUITE_P(
	InpxTests,
	InpxTest,
	testing::Values(
		InpxTestData {
			// empty
			{                                             },
			{ { "select count(42) from Books", { { "0" } } } },
},
		InpxTestData { // empty (no inp)
		               { ":/common/version.info" },
		               { { "select count(42) from Books", { { "0" } } } } },
		InpxTestData { // empty (no inp)
		               { ":/common/version.info", ":/common/version.info" },
		               { { "select count(42) from Books", { { "0" } } } } },
		InpxTestData { // one book
		               { ":/common/collection.info", ":/common/version.info", ":/0001/1.inp" },
		               { { "select count(42) from Books", { { "1" } } } },
		               { .folders = 1, .authors = 1, .books = 1, .genres = 1 } },
		InpxTestData { // one series
		               { ":/common/collection.info", ":/common/version.info", ":/0002/1.inp" },
		               { { "select count(42) from Books", { { "1" } } } },
		               { .folders = 1, .authors = 1, .series = 1, .books = 1, .genres = 1 } },
		InpxTestData { // multi series (2)
		               { ":/common/collection.info", ":/common/version.info", ":/0003/1.inp" },
		               { { "select count(42) from Books", { { "1" } } } },
		               { .folders = 1, .authors = 1, .series = 2, .books = 1, .genres = 1 } },
		InpxTestData { // author with nick
		               { ":/common/collection.info", ":/common/version.info", ":/0004/1.inp" },
		               {
						   { "select count(42) from Books", { { "2" } } },
						   { "select count(42) from Authors where NickName is not null", { { "1" } } },
					   },
		               { .folders = 1, .authors = 1, .series = 1, .books = 2, .genres = 1 } },
		InpxTestData { // usual parsing - inps, authors, books, series, etc
		               { ":/common/collection.info", ":/common/version.info", ":/common/structure.info", ":/0005/1.inp", ":/0005/2.inp", ":/0005/3.inp", ":/0005/4.inp", ":/0005/5.inp" },
		               {
						   { "select count(42) from Books", { { "76" } } },
						   { "select count(42) from Authors", { { "122" } } },
						   { "select count(42) from Series", { { "46" } } },
						   { "select count(42) from Folders", { { "5" } } },
						   { "select NickName, count(42) from Authors group by NickName order by NickName",
						     {
								 { "", "121" },
								 { "gromoff", "1" },
							 } },
						   { "select nullif(LibRate, ''), count(42) from Books group by LibRate order by LibRate",
						     {
								 { "1", "7" },
								 { "2", "1" },
								 { "3", "4" },
								 { "4", "1" },
								 { "", "63" },
							 } },
						   { "select Lang, count(42) from Books group by Lang order by Lang",
						     {
								 { "en", "10" },
								 { "ru", "65" },
								 { "uk", "1" },
							 } },
						   { "select nullif(Year, ''), count(42) from Books group by Year order by Year",
						     {
								 { "", "11" },
								 { "1975", "1" },
								 { "2005", "2" },
								 { "2006", "2" },
								 { "2009", "1" },
								 { "2010", "2" },
								 { "2014", "1" },
								 { "2018", "3" },
								 { "2019", "3" },
								 { "2020", "2" },
								 { "2021", "3" },
								 { "2022", "4" },
								 { "2023", "8" },
								 { "2024", "6" },
								 { "2025", "7" },
								 { "2026", "20" },
							 } },
						   { "select nullif(Ext, ''), count(42) from Books group by Ext order by Ext",
						     {
								 { ".epub", "30" },
								 { ".fb2", "46" },
							 } },
						   { "select nullif(IsDeleted, ''), count(42) from Books group by IsDeleted order by IsDeleted",
						     {
								 { "0", "72" },
								 { "1", "4" },
							 } },
						   { "select nullif(SourceLib, ''), count(42) from Books group by SourceLib order by SourceLib",
						     {
								 { "flibusta", "48" },
								 { "librusec", "28" },
							 } },
					   },
		               { .folders = 5, .authors = 122, .series = 46, .books = 76, .keywords = 60, .genres = 1 } },
		InpxTestData { // update collection by author with nick
		               { ":/common/collection.info", ":/common/version.info", ":/0004/1.inp" },
		               {
						   { "select count(42) from Books", { { "2" } } },
						   { "select count(42) from Authors where NickName is not null", { { "1" } } },
					   },
		               { .folders = 1, .authors = 1, .series = 1, .books = 2, .genres = 1 },
		               { ":/common/collection.info", ":/common/version.info", ":/0004/1.inp", ":/0006/6.inp" },
		               {
						   { "select count(42) from Books", { { "3" } } },
						   { "select count(42) from Authors where NickName is not null", { { "1" } } },
					   },
		               { .folders = 1, .books = 1, .genres = 1 } },
		InpxTestData { // update collection, old data update detected
		               { ":/common/collection.info", ":/common/version.info", ":/0001/1.inp" },
		               {
						   { "select count(42) from Books", { { "1" } } },
					   },
		               { .folders = 1, .authors = 1, .books = 1, .genres = 1 },
		               { ":/common/collection.info", ":/common/version.info", ":/0002/1.inp", ":/0006/6.inp" },
		               {
						   { "select count(42) from Books", { { "2" } } },
					   },
		               { .folders = 1, .authors = 1, .books = 1, .genres = 1, .oldDataUpdateFound = true } }
	)
);

auto GetInpxSetter(const std::vector<QString>& files)
{
	return [&](IZipFileController& zipFiles) {
		for (const auto& file : files)
		{
			QFile stream(file);
			ASSERT_TRUE(stream.open(QIODevice::ReadOnly));
			zipFiles.AddFile(QFileInfo(file).fileName(), stream.readAll());
		}
	};
}

auto GetResultChecker(const std::vector<InpxTestData::Check>& checks, const Inpx::UpdateResult& updateResult)
{
	return [&](const Inpx::UpdateResult& result, DB::IDatabase& db) {
		EXPECT_EQ(result, updateResult);
		for (const auto& [queryText, results] : checks)
		{
			const auto query = db.CreateQuery(queryText);
			if (results.empty())
			{
				query->Execute();
				EXPECT_TRUE(query->Eof());
				continue;
			}

			size_t n = 0;
			for (const auto [r, q] : std::views::zip(results, *query))
			{
				++n;
				for (auto&& [value, index] : std::views::zip(r, std::views::iota(0)))
				{
					if (value.empty())
					{
						EXPECT_TRUE(query->IsNull(static_cast<size_t>(index)));
					}
					else
					{
						const auto queryValue = query->Get<const char*>(static_cast<size_t>(index));
						EXPECT_STREQ(queryValue, value.data());
					}
				}
			}
			EXPECT_EQ(n, results.size());
		}
	};
}

void CreateBadArchive(const QTemporaryDir& dir, const QString& fileName)
{
	QFile f(dir.filePath(fileName));
	ASSERT_TRUE(f.open(QIODevice::WriteOnly));
	f.write("12345");
}

TEST_P(InpxTest, Create)
{
	const auto& [files, checks, result, updateFiles, updateChecks, updateResult] = GetParam();

	const auto db = Process(GetInpxSetter(files), GetResultChecker(checks, result), &Inpx::Parser::CreateNewCollection);
	if (!updateFiles.empty())
		Process(GetInpxSetter(updateFiles), GetResultChecker(updateChecks, updateResult), &Inpx::Parser::UpdateCollection);
}

TEST(InpxTest, SkipLostBooksNoArchives)
{
	const InpxTestData testData {
		.files = { ":/common/collection.info", ":/common/version.info", ":/0001/1.inp" }
	};

	const QTemporaryDir temporaryDir;

	const auto db =
		Process(GetInpxSetter(testData.files), GetResultChecker(testData.checks, testData.result), &Inpx::Parser::CreateNewCollection, temporaryDir.path(), Inpx::CreateCollectionMode::SkipLostBooks);
}

TEST(InpxTest, SkipLostBooksBadArchive)
{
	const InpxTestData testData {
		.files  = { ":/common/collection.info", ":/common/version.info", ":/0001/1.inp" },
		.result = { .genres = 1, .badFolders = { "1.zip" } }
	};

	const QTemporaryDir temporaryDir;
	CreateBadArchive(temporaryDir, "1.zip");

	const auto db =
		Process(GetInpxSetter(testData.files), GetResultChecker(testData.checks, testData.result), &Inpx::Parser::CreateNewCollection, temporaryDir.path(), Inpx::CreateCollectionMode::SkipLostBooks);
}

TEST(InpxTest, BadArchive)
{
	const InpxTestData testData {
		.files  = { ":/common/collection.info", ":/common/version.info", ":/0001/1.inp" },
		.result = { .folders = 1, .authors = 1, .books = 1, .genres = 1, .badFolders = { "1.zip" } }
	};

	const QTemporaryDir temporaryDir;
	CreateBadArchive(temporaryDir, "1.zip");

	const auto db = Process(GetInpxSetter(testData.files), GetResultChecker(testData.checks, testData.result), &Inpx::Parser::CreateNewCollection, temporaryDir.path());
}

} // namespace HomeCompa::Flibrary::Test
