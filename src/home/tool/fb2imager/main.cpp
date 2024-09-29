#include <deque>

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTranslator>

#include <plog/Log.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ConsoleAppender.h>

#include "interface/constants/Localization.h"
#include "logging/init.h"
#include "logging/LogAppender.h"
#include "util/Settings.h"

#include "Fb2Parser.h"

#include "config/version.h"

using namespace HomeCompa;
using namespace fb2imager;

namespace {

constexpr auto APP_ID = "fb2imager";

constexpr auto CONTEXT = APP_ID;
constexpr auto APP_DESCRIPTION = QT_TRANSLATE_NOOP("fb2imager", "fb2imager extracts images from *.fb2");
constexpr auto APP_STARTED = QT_TRANSLATE_NOOP("fb2imager", "%1 started");
constexpr auto APP_FINISHED = QT_TRANSLATE_NOOP("fb2imager", "%1 finished successfully");
constexpr auto APP_FINISHED_WITH_ERRORS = QT_TRANSLATE_NOOP("fb2imager", "%1 finished with errors");
constexpr auto APP_FAILED = QT_TRANSLATE_NOOP("fb2imager", "%1 failed");
constexpr auto APP_FAILED_WITH_ERROR = QT_TRANSLATE_NOOP("fb2imager", "%1 failed: %2");
constexpr auto FILE_PROCESSING = QT_TRANSLATE_NOOP("fb2imager", "%1 processing");
constexpr auto FILE_DONE = QT_TRANSLATE_NOOP("fb2imager", "%1 done");
constexpr auto FILE_PROCESSING_FAILED = QT_TRANSLATE_NOOP("fb2imager", "%1 processing failed");
constexpr auto FILE_PROCESSING_FAILED_WITH_ERRORS = QT_TRANSLATE_NOOP("fb2imager", "%1 processing failed: %2");
constexpr auto CANNOT_OPEN_FILE = QT_TRANSLATE_NOOP("fb2imager", "Cannot open file");
constexpr auto CANNOT_CREATE_FOLDER = QT_TRANSLATE_NOOP("fb2imager", "Cannot create folder %1");
constexpr auto CANNOT_WRITE_FILE = QT_TRANSLATE_NOOP("fb2imager", "Cannot write to %1");

TR_DEF

void ProcessFile(const QString & file)
{
	QFile stream(file);
	if (!stream.open(QIODevice::ReadOnly))
		throw std::ios_base::failure(Tr(CANNOT_OPEN_FILE).toStdString());

	const QFileInfo fileInfo(file);
	const QDir dir(fileInfo.dir().filePath(fileInfo.completeBaseName()));
	if (!dir.exists() && !dir.mkpath("."))
		throw std::ios_base::failure(Tr(CANNOT_CREATE_FOLDER).arg(dir.dirName()).toStdString());

	auto binaryCallback = [&] (const QString& name, const QByteArray& body)
	{
	QFile image(dir.filePath(name));
		if (!image.open(QIODevice::WriteOnly))
			throw std::ios_base::failure(Tr(CANNOT_WRITE_FILE).arg(image.fileName()).toStdString());

		image.write(body);
	return true;
	};

	Fb2Parser(stream, file, std::move(binaryCallback)).Parse();
}

void ProcessFile(const QString & file, std::atomic_bool & hasErrors)
{
	try
	{
		PLOGI << Tr(FILE_PROCESSING).arg(file);
		ProcessFile(file);
		PLOGI << Tr(FILE_DONE).arg(file);
	}
	catch (const std::exception & ex)
	{
		PLOGE << Tr(FILE_PROCESSING_FAILED_WITH_ERRORS).arg(file).arg(ex.what());
	}
	catch (...)
	{
	PLOGE << Tr(FILE_PROCESSING_FAILED).arg(file);
	}

	hasErrors = true;
}

bool ProcessFiles(QStringList files)
{
	std::atomic_bool hasErrors = false;

	std::deque queue(std::make_move_iterator(files.begin()), std::make_move_iterator(files.end()));
	for (const auto & file : queue)
		ProcessFile(file, hasErrors);

	return hasErrors;
}

QStringList ProcessWildCard(const QString & wildCard)
{
	const QFileInfo fileInfo(wildCard);
	const QDir dir(fileInfo.dir());
	return dir.entryList(QStringList() << fileInfo.fileName(), QDir::Files);
}

bool run(int argc, char * argv[])
{
	const QCoreApplication app(argc, argv);
	QCoreApplication::setApplicationName(APP_ID);
	QCoreApplication::setApplicationVersion(PRODUCT_VERSION);

	PropagateConstPtr<ISettings> settings(std::unique_ptr<ISettings>{std::make_unique<Settings>(COMPANY_ID, Flibrary::PRODUCT_ID)});
	const auto translators = Loc::LoadLocales(*settings);

	QCommandLineParser parser;
	parser.setApplicationDescription(Tr(APP_DESCRIPTION));
	parser.addHelpOption();
	parser.addVersionOption();
	parser.process(app);

	QStringList files;
	for (const auto & wildCard : parser.positionalArguments())
		files << ProcessWildCard(wildCard);

	return ProcessFiles(std::move(files));
}

}

int main(const int argc, char * argv[])
{
	plog::ConsoleAppender<plog::TxtFormatter> consoleAppender;
	Log::LogAppender logConsoleAppender(&consoleAppender);
	Log::LoggingInitializer logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, APP_ID).toStdWString());
	PLOGI << Tr(APP_STARTED).arg(APP_ID);

	try
	{
		if (run(argc, argv))
			PLOGI << Tr(APP_FINISHED_WITH_ERRORS).arg(APP_ID);
		else
			PLOGW << Tr(APP_FINISHED).arg(APP_ID);
		return 0;
	}
	catch(const std::exception & ex)
	{
		PLOGE << Tr(APP_FAILED_WITH_ERROR).arg(APP_ID).arg(ex.what());
	}
	catch (...)
	{
		PLOGE << Tr(APP_FAILED).arg(APP_ID);
	}
	return 1;
}
