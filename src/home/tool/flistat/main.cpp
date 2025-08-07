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

void go(const int argc, char* argv[])
{
	if (argc < 3)
		throw std::invalid_argument("usage:\nflistat.exe database_path statistics_path");

	QFile inp(argv[2]);
	if (!inp.open(QIODevice::ReadOnly))
		throw std::invalid_argument(std::format("cannot read {}", argv[1]));

	const auto totalSize = inp.size();

	const auto db = Create(DB::Factory::Impl::Sqlite, std::format("path={}", argv[1]));
	const auto tr = db->CreateTransaction();
	{
		const auto command = tr->CreateCommand(R"(INSERT INTO Image (Folder, FileName, OrdNum, PixelType, Size, Width, Height, Hash) VALUES (?, ?, ?, ?, ?, ?, ?, ?))");

		long long percents = 0;

		QTextStream stream(&inp);
		QString line;
		size_t counter = 0;
		while (stream.readLineInto(&line))
		{
			const auto data = line.split(',');
			assert(data.size() == 8);
			command->Bind(0, data[0].toStdString());
			command->Bind(1, data[1].toStdString());
			if (data[2].isEmpty())
				command->Bind(2);
			else
				command->Bind(2, data[2].toInt());
			command->Bind(3, data[3].toInt());
			command->Bind(4, data[4].toLongLong());
			command->Bind(5, data[5].toInt());
			command->Bind(6, data[6].toInt());
			command->Bind(7, data[7].toStdString());
			if (!command->Execute())
				PLOGE << "error line: " << counter;
			++counter;

			const auto position = inp.pos();
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
	Log::LoggingInitializer logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, APP_ID).toStdWString());
	plog::ConsoleAppender<Util::LogConsoleFormatter> consoleAppender;
	Log::LogAppender logConsoleAppender(&consoleAppender);

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
