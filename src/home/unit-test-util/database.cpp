#include "database.h"

#include <QBuffer>
#include <QEventLoop>
#include <QFileInfo>

#include "fnd/StrUtil.h"

#include "database/interface/IDatabase.h"

#include "interface/logic/IDatabaseProvider.h"

#include "database/factory/Factory.h"
#include "inpx/InpxConstant.h"
#include "inpx/inpx.h"
#include "util/Fb2InpxParser.h"

#include "log.h"
#include "zip.h"

namespace HomeCompa::Test {

QByteArray GetBookData()
{
	QStringList data {
		"LastName,FirstName,MiddleName [NickName]:", "sf:", "BookTitle", "SeriesTitle", "42", "100500", "2001000", "100500", "0", "fb2", "1974-01-01", "0", "sq", "4", "story:", "2006", "flibusta"
	};
	return data.join(Util::Fb2InpxParser::FIELDS_SEPARATOR).append(Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8();
}

void AddFile(IZipFileController& zipFiles, const QString& fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly))
		throw std::ios_base::failure(std::format("cannot read from {}", fileName));
	zipFiles.AddFile(QFileInfo(fileName).fileName(), file.readAll());
}

template <Details::ExportedDatabaseType T>
std::unique_ptr<T> CreateDatabase()
{
	try
	{
		constexpr auto dbPath = "file:my_shared_mem_db?mode=memory&cache=shared";
		auto           db     = DB::Factory::Create<DB::IDatabaseWritable>(DB::IFactory::Impl::Sqlite, std::format(R"(path="{}")", dbPath));

		QByteArray bytes;
		{
			const auto zipFiles = Zip::CreateZipFileController();
			QBuffer    zipStream(&bytes);
			zipStream.open(QIODevice::WriteOnly);
			Zip zip(zipStream, Zip::Format::Zip);
			zip.SetProperty(Zip::PropertyId::CompressionMethod, QVariant::fromValue(Zip::CompressionMethod::Copy));
			zip.SetProperty(Zip::PropertyId::CompressionLevel, QVariant::fromValue(Zip::CompressionLevel::None));

			AddFile(*zipFiles, ":/database/collection.info");
			AddFile(*zipFiles, ":/database/version.info");
			AddFile(*zipFiles, ":/database/structure.info");
			zipFiles->AddFile("1.inp", GetBookData());

			zip.Write(*zipFiles);
		}

		QEventLoop           eventLoop;
		Inpx::Parser         parser;
		Inpx::Parser::IniMap ini {
			{                        DB_PATH,                                                     dbPath },
			{               DB_CREATE_SCRIPT,                             ":/data/CreateCollection.json" },
			{               DB_UPDATE_SCRIPT,                             ":/data/UpdateCollection.json" },
			{                    GENRES_PATH,                                       ":/data/genres.json" },
			{ SET_DATABASE_VERSION_STATEMENT, Flibrary::IDatabaseProvider::GetDatabaseVersionStatement() },
			{                      INPX_PATH,            QString::fromUtf8(bytes.toBase64()) + "_BASE64" },
			{                 ARCHIVE_FOLDER,                                                   "folder" },
		};

		parser.CreateNewCollection(std::move(ini), Inpx::CreateCollectionMode::None, [&](const Inpx::UpdateResult&) {
			eventLoop.exit();
		});

		eventLoop.exec();

		return DB::Factory::Create<T>(DB::IFactory::Impl::Sqlite, std::format(R"(path="{}")", dbPath));
	}
	catch (const std::exception& ex)
	{
		PLOGE << ex.what();
	}
	catch (...)
	{
		PLOGE << "unknown error";
	}

	return {};
}

template UNIT_TEST_UTIL_EXPORT std::unique_ptr<DB::IDatabase> CreateDatabase();
template UNIT_TEST_UTIL_EXPORT std::unique_ptr<DB::IDatabaseWritable> CreateDatabase();

} // namespace HomeCompa::Test
