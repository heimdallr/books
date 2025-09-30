#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

#include <plog/Appenders/ConsoleAppender.h>

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/ITransaction.h"

#include "database/factory/Factory.h"
#include "logging/LogAppender.h"
#include "logging/init.h"
#include "util/LogConsoleFormatter.h"

#include "log.h"

#include "config/version.h"

using namespace HomeCompa;

namespace
{

constexpr auto APP_ID = "flistat";

void CreateDatabaseSchema(DB::IDatabase& db)
{
	static constexpr const char* COMMANDS[] {
		R"(
CREATE TABLE Image (
    Folder    VARCHAR (200) NOT NULL,
    FileName  VARCHAR (255) NOT NULL,
    ImageID   VARCHAR (255) NOT NULL,
    FailInfo  VARCHAR (20),
	IsCover   INTEGER       NOT NULL,
    PixelType INTEGER,
    Size      BIGINT,
    Width     INTEGER,
    Height    INTEGER,
    Hash      VARCHAR (50)  NOT NULL
)
)",
		"CREATE INDEX IX_Image_Hash ON Image (Hash)",
		"CREATE INDEX IX_Image_Folder ON Image (Folder)",
		"CREATE INDEX IX_Image_FileName ON Image (FileName)",
	};

	const auto tr = db.CreateTransaction();
	for (const auto* command : COMMANDS)
		tr->CreateCommand(command)->Execute();
	tr->Commit();
}

std::unique_ptr<DB::IDatabase> CreateDatabase(const std::string_view fileName)
{
	const auto dbExists     = QFile(QString::fromStdString(fileName.data())).exists();
	const auto dbParameters = std::format("path={};flag={}", fileName, dbExists ? "READWRITE" : "CREATE");
	auto       db           = Create(DB::Factory::Impl::Sqlite, dbParameters);
	if (!dbExists)
		CreateDatabaseSchema(*db);
	return db;
}

template <std::integral T>
using FromString = T (QString::*)(bool*, int) const;

template <std::integral T>
void Bind(DB::ICommand& command, const size_t index, const QString& str, const FromString<T>& functor, const T nullValue = 0)
{
	bool ok = false;
	if (const auto value = std::invoke(functor, std::cref(str), &ok, 10); ok && value != nullValue)
		command.Bind(index, value);
	else
		command.Bind(index);
}

void go(const int argc, char* argv[])
{
	if (argc < 3)
		throw std::invalid_argument("usage:\nflistat.exe database_path statistics_path");

	QFile inp(argv[2]);
	if (!inp.open(QIODevice::ReadOnly))
		throw std::invalid_argument(std::format("cannot read {}", argv[1]));

	const auto totalSize = inp.size();

	const auto db = CreateDatabase(argv[1]);
	const auto tr = db->CreateTransaction();
	{
		const auto command = tr->CreateCommand(R"(INSERT INTO Image (Folder, FileName, ImageID, FailInfo, IsCover, PixelType, Size, Width, Height, Hash) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?))");

		long long percents = 0;

		QTextStream stream(&inp);
		QString     line;
		size_t      counter = 0;
		while (stream.readLineInto(&line))
		{
			if (line.startsWith('#'))
				continue;

			const auto data = line.split('|');
			if (data.size() != 10)
			{
				PLOGE << line;
				assert(false && "bad line");
				continue;
			}
			command->Bind(0, data[0].toStdString());
			command->Bind(1, data[1].toStdString());
			command->Bind(2, data[2].toStdString());
			if (data[3].isEmpty())
				command->Bind(3);
			else
				command->Bind(3, data[3].toStdString());
			command->Bind(4, data[4].toInt());
			Bind(*command, 5, data[5], qOverload<bool*, int>(&QString::toInt), -1);
			Bind(*command, 6, data[6], qOverload<bool*, int>(&QString::toLongLong));
			Bind(*command, 7, data[7], qOverload<bool*, int>(&QString::toInt));
			Bind(*command, 8, data[8], qOverload<bool*, int>(&QString::toInt));
			command->Bind(9, data[9].toStdString());
			if (!command->Execute())
				PLOGE << "error line: " << counter;
			++counter;

			const auto position        = inp.pos();
			const auto currentPercents = 100LL * position / totalSize;
			if (percents == currentPercents)
				continue;

			percents = currentPercents;
			PLOGV << counter << " rows inserted " << percents << "%";
		}
	}
	tr->Commit();
}

} // namespace

int main(const int argc, char* argv[])
{
	Log::LoggingInitializer                          logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, APP_ID).toStdWString());
	plog::ConsoleAppender<Util::LogConsoleFormatter> consoleAppender;
	Log::LogAppender                                 logConsoleAppender(&consoleAppender);

	try
	{
		go(argc, argv);
		return 0;
	}
	catch (const std::exception& ex)
	{
		PLOGE << ex.what();
	}
	catch (...)
	{
		PLOGE << "Unknown error";
	}

	return 1;
}
