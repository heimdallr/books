#include <deque>

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QPixmap>
#include <QStandardPaths>
#include <QTranslator>
#include <QBuffer>

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
constexpr auto MAX_WIDTH_OPTION_NAME = "max-width";
constexpr auto MAX_HEIGHT_OPTION_NAME = "max-height";

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
constexpr auto BAD_IMAGE = QT_TRANSLATE_NOOP("fb2imager", "Incorrect image %1: %2");
constexpr auto CANNOT_WRITE_FILE = QT_TRANSLATE_NOOP("fb2imager", "Cannot write to %1");
constexpr auto MAX_WIDTH = QT_TRANSLATE_NOOP("fb2imager", "Maximum image width");
constexpr auto MAX_HEIGHT = QT_TRANSLATE_NOOP("fb2imager", "Maximum image height");
constexpr auto WIDTH = QT_TRANSLATE_NOOP("fb2imager", "width");
constexpr auto HEIGHT = QT_TRANSLATE_NOOP("fb2imager", "height");
constexpr auto DESTINATION_FOLDER = QT_TRANSLATE_NOOP("fb2imager", "Destination folder");
constexpr auto FOLDER = QT_TRANSLATE_NOOP("fb2imager", "folder");
constexpr auto NEED_DESTINATION_FOLDER = QT_TRANSLATE_NOOP("fb2imager", "Destination folder required");

TR_DEF

bool ProcessFile(const QString & inputFilePath, const QDir & dstDir, const int maxWidth, const int maxHeight)
{
	QFile input(inputFilePath);
	if (!input.open(QIODevice::ReadOnly))
		throw std::ios_base::failure(Tr(CANNOT_OPEN_FILE).toStdString());

	const QFileInfo fileInfo(inputFilePath);

	const auto outputFilePath = dstDir.filePath(fileInfo.fileName());
	QFile output(outputFilePath);
	if (!output.open(QIODevice::WriteOnly))
		throw std::ios_base::failure(Tr(CANNOT_WRITE_FILE).arg(outputFilePath).toStdString());

	const QDir dir(dstDir.filePath(fileInfo.completeBaseName()));
	if (!dir.exists() && !dir.mkpath("."))
		throw std::ios_base::failure(Tr(CANNOT_CREATE_FOLDER).arg(dir.dirName()).toStdString());


	bool hasError = false;
	auto binaryCallback = [&] (const QString & name, QByteArray body)
	{
		QBuffer buffer(&body);
		buffer.open(QBuffer::ReadOnly);

		QImageReader imageReader(&buffer);
		auto image = imageReader.read();
		if (image.isNull())
		{
			PLOGW << Tr(BAD_IMAGE).arg(name).arg(imageReader.errorString());
			hasError = true;
			return true;
		}
		image = image.scaled(maxWidth, maxHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);

		if (!image.save(dir.filePath(name)))
		{
			PLOGW << Tr(CANNOT_WRITE_FILE).arg(name).arg(imageReader.errorString());
			hasError = true;
		}

		return true;
	};

	Fb2Parser(inputFilePath, input, output, std::move(binaryCallback)).Parse();

	return hasError;

}

void ProcessFile(const QString & file, const QDir & dstDir, const int maxWidth, const int maxHeight, std::atomic_bool & hasErrors)
{
	try
	{
		PLOGI << Tr(FILE_PROCESSING).arg(file);
		if (ProcessFile(file, dstDir, maxWidth, maxHeight))
		{
			PLOGE << Tr(FILE_PROCESSING_FAILED).arg(file);
		}
		else
		{
			PLOGI << Tr(FILE_DONE).arg(file);
			return;
		}
		return;
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

bool ProcessFiles(QStringList files, const QDir& dstDir, const int maxWidth, const int maxHeight)
{
	std::atomic_bool hasErrors = false;

	std::deque queue(std::make_move_iterator(files.begin()), std::make_move_iterator(files.end()));
	for (const auto & file : queue)
		ProcessFile(file, dstDir, maxWidth, maxHeight, hasErrors);

	return hasErrors;
}

QStringList ProcessWildCard(const QString & wildCard)
{
	const QFileInfo fileInfo(wildCard);
	const QDir dir(fileInfo.dir());
	const auto files = dir.entryList(QStringList() << fileInfo.fileName(), QDir::Files);
	QStringList result;
	result.reserve(files.size());
	std::ranges::transform(files, std::back_inserter(result), [&] (const auto & file) { return dir.filePath(file); });
	return result;
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
	parser.addOptions({
		{ MAX_WIDTH_OPTION_NAME, Tr(MAX_WIDTH)         , Tr(WIDTH)  },
		{ MAX_HEIGHT_OPTION_NAME, Tr(MAX_HEIGHT)        , Tr(HEIGHT) },
		{ { QString(FOLDER[0]), FOLDER                 }, Tr(DESTINATION_FOLDER), Tr(FOLDER) },
		});
	parser.process(app);

	const auto getMaxSizeOption = [&] (const char * name)
	{
		bool ok = false;
		const auto value = parser.value(name).toInt(&ok);
		return ok ? value : std::numeric_limits<int>::max();
	};

	const auto maxWidth = getMaxSizeOption(MAX_WIDTH_OPTION_NAME);
	const auto maxHeight = getMaxSizeOption(MAX_HEIGHT_OPTION_NAME);
	const auto destinationFolder = parser.value(FOLDER);
	if (destinationFolder.isEmpty())
	{
		PLOGE << Tr(NEED_DESTINATION_FOLDER).toStdString();
		parser.showHelp(1);
	}

	const QDir dstDir(destinationFolder);
	if (!dstDir.exists() && !dstDir.mkpath("."))
		throw std::ios_base::failure(Tr(CANNOT_CREATE_FOLDER).arg(destinationFolder).toStdString());

	QStringList files;
	for (const auto & wildCard : parser.positionalArguments())
		files << ProcessWildCard(wildCard);

	return ProcessFiles(std::move(files), dstDir, maxWidth, maxHeight);
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
