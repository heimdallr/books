#include <queue>
#include <ranges>

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QPixmap>
#include <QStandardPaths>
#include <QBuffer>
#include <QProcess>

#include <plog/Log.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ConsoleAppender.h>

#include "common/Constant.h"

#include "fnd/NonCopyMovable.h"

#include "logging/init.h"
#include "logging/LogAppender.h"

#include "zip.h"

#include "Fb2Parser.h"

#include "config/version.h"

using namespace HomeCompa;
using namespace fb2cut;

namespace {

constexpr auto APP_ID = "fb2cut";
constexpr auto MAX_WIDTH_OPTION_NAME = "max-width";
constexpr auto MAX_HEIGHT_OPTION_NAME = "max-height";
constexpr auto QUALITY_OPTION_NAME = "quality";
constexpr auto MAX_THREAD_COUNT_OPTION_NAME = "threads";
constexpr auto NO_IMAGES_OPTION_NAME = "no-images";
constexpr auto COVERS_ONLY_OPTION_NAME = "covers-only";
constexpr auto SEVEN_ZIP_OPTION_NAME = "7z";

constexpr auto MAX_WIDTH = "Maximum image width";
constexpr auto MAX_HEIGHT = "Maximum image height";
constexpr auto COMPRESSION_QUALITY = "Compression quality [0, 100] or -1 for default compression quality";
constexpr auto DESTINATION_FOLDER = "Destination folder (required)";
constexpr auto MAX_THREAD_COUNT = "Maximum number of CPU threads";
constexpr auto NO_IMAGES = "Don't save image";
constexpr auto COVERS_ONLY = "Save covers only";
constexpr auto SEVEN_ZIP = "Path to 7z executable";

constexpr auto WIDTH = "width [%1]";
constexpr auto HEIGHT = "height [%1]";
constexpr auto QUALITY = "quality [-1]";
constexpr auto THREADS = "threads [%1]";
constexpr auto FOLDER = "folder";
constexpr auto PATH = "path";

using DataItem = std::pair<QString, QByteArray>;
using DataItems = std::queue<DataItem>;

struct Settings
{
	int maxWidth { std::numeric_limits<int>::max() };
	int maxHeight { std::numeric_limits<int>::max() };
	int quality { -1 };
	int maxThreadCount { static_cast<int>(std::thread::hardware_concurrency()) };
	bool covers { true };
	bool images { true };
	QDir dstDir;
	QString sevenZipPath;
};

bool WriteImagesImpl(Zip & zip, const std::vector<DataItem> & images)
{
	return !std::ranges::all_of(images, [&] (const auto & item)
	{
		return zip.Write(item.first).write(item.second) == item.second.size();
	});
}

bool WriteImages(const std::unique_ptr<Zip> & zip, std::mutex & guard, const QString & name, std::vector<DataItem> & images)
{
	if (images.empty())
		return false;

	assert(zip);

	PLOGI << QString("zipping %1: %2").arg(name).arg(images.size());

	std::scoped_lock lock(guard);
	const auto result = WriteImagesImpl(*zip, images);
	images.clear();

	return result;
}

void WriteError(const QDir & dir, std::mutex & guard, const QString & name, const QByteArray & body)
{
	std::scoped_lock lock(guard);

	const auto filePath = dir.filePath(QString("error/%1.bad").arg(name));
	const QDir imgDir = QFileInfo(filePath).dir();
	if (!imgDir.exists() && !imgDir.mkpath("."))
	{
		PLOGE << QString("Cannot create folder %1").arg(imgDir.path());
		return;
	}

	QFile file(filePath);
	if (!file.open(QIODevice::WriteOnly))
	{
		PLOGE << QString("Cannot write to %1").arg(filePath);
		return;
	}

	if (file.write(body) != body.size())
		PLOGE << QString("%1 written with errors").arg(filePath);
}

class Worker
{
	NON_COPY_MOVABLE(Worker)

public:
	Worker(const Settings & settings
		, const int totalFiles
		, std::condition_variable & queueCondition
		, std::mutex & queueGuard
		, DataItems & queue
		, std::mutex & fileSystemGuard
		, std::atomic_bool & hasError
		, std::atomic_int & queueSize
		, std::atomic_int & filesCount
		, std::mutex & zipCoversGuard
		, std::mutex & zipImagesGuard
		, const std::unique_ptr<Zip> & zipCovers
		, const std::unique_ptr<Zip> & zipImages

	)
		: m_settings { settings }
		, m_totalFiles { totalFiles }
		, m_queueCondition { queueCondition }
		, m_queueGuard { queueGuard }
		, m_queue { queue }
		, m_fileSystemGuard { fileSystemGuard }
		, m_hasError { hasError }
		, m_queueSize { queueSize }
		, m_filesCount { filesCount }
		, m_zipCoversGuard { zipCoversGuard }
		, m_zipImagesGuard { zipImagesGuard }
		, m_zipCovers { zipCovers }
		, m_zipImages { zipImages }
		, m_thread { &Worker::Process, this }
	{
	}

	~Worker()
	{
		if (m_thread.joinable())
			m_thread.join();
	}

private:
	void Process()
	{
		while (true)
		{
			QString name;
			QByteArray body;
			{
				std::unique_lock queueLock(m_queueGuard);
				m_queueCondition.wait(queueLock, [&] ()
				{
					return !m_queue.empty();
				});

				auto [f, d] = std::move(m_queue.front());
				m_queue.pop();
				--m_queueSize;
				m_queueCondition.notify_all();
				name = std::move(f);
				body = std::move(d);
			}

			if (body.isEmpty())
				break;

			m_hasError = ProcessFile(name, body) || m_hasError;
		}

		m_hasError = WriteImages(m_zipCovers, m_zipCoversGuard, Global::COVERS, m_covers) || m_hasError;
		m_hasError = WriteImages(m_zipImages, m_zipImagesGuard, Global::IMAGES, m_images) || m_hasError;
	}

	bool ProcessFile(const QString & inputFilePath, QByteArray & inputFileBody)
	{
		PLOGV << QString("%1(%2), parsing %3").arg(++m_filesCount).arg(m_totalFiles).arg(inputFilePath);

		QBuffer input(&inputFileBody);
		input.open(QIODevice::ReadOnly);

		const QFileInfo fileInfo(inputFilePath);
		const auto outputFilePath = m_settings.dstDir.filePath(fileInfo.fileName());

		const auto bodyOutput = ParseFile(inputFilePath, input);
		if (bodyOutput.isEmpty())
		{
			PLOGW << QString("Cannot parse %1").arg(outputFilePath);
			return true;
		}

		std::scoped_lock fileSystemLock(m_fileSystemGuard);
		QFile bodyFle(outputFilePath);
		if (!bodyFle.open(QIODevice::WriteOnly))
		{
			PLOGW << QString("Cannot write body to %1").arg(outputFilePath);
			return true;
		}

		return bodyFle.write(bodyOutput) != bodyOutput.size();
	}

	QByteArray ParseFile(const QString & inputFilePath, QIODevice & input)
	{
		const QFileInfo fileInfo(inputFilePath);

		QByteArray bodyOutput;
		QBuffer output(&bodyOutput);
		output.open(QIODevice::WriteOnly);

		auto binaryCallback = [&] (const QString & name, const bool isCover, QByteArray body)
		{
			if ((isCover && !m_settings.covers) || (!isCover && !m_settings.images))
				return;

			QBuffer buffer(&body);
			buffer.open(QBuffer::ReadOnly);

			auto imageFile = isCover
				? QString("%1").arg(fileInfo.completeBaseName())
				: QString("%1/%2").arg(fileInfo.completeBaseName()).arg(name);

			QImageReader imageReader(&buffer);
			auto image = imageReader.read();
			if (image.isNull())
				return AddError(imageFile, body, QString("Incorrect image %1.%2: %3").arg(name).arg(imageFile).arg(imageReader.errorString()));

			if (image.width() > m_settings.maxWidth || image.height() > m_settings.maxHeight)
				image = image.scaled(m_settings.maxWidth, m_settings.maxHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);

			QByteArray imageBody;
			{
				QBuffer imageOutput(&imageBody);

				if (!image.save(&imageOutput, "jpeg", m_settings.quality))
					return AddError(imageFile, body, QString("Cannot compress image %1.%2").arg(name).arg(imageFile));
			}
			auto & storage = isCover ? m_covers : m_images;
			storage.emplace_back(std::move(imageFile), std::move(imageBody));
		};

		Fb2Parser(inputFilePath, input, output, std::move(binaryCallback)).Parse();

		return bodyOutput;
	}

	void AddError(const QString & file, const QByteArray & body, const QString & errorText) const
	{
		PLOGW << errorText;
		WriteError(m_settings.dstDir, m_fileSystemGuard, file, body);
		m_hasError = true;
	}

private:
	const Settings & m_settings;
	const int m_totalFiles;

	std::condition_variable & m_queueCondition;
	std::mutex & m_queueGuard;
	DataItems & m_queue;
	std::mutex & m_fileSystemGuard;

	std::atomic_bool & m_hasError;
	std::atomic_int & m_queueSize, & m_filesCount;

	std::mutex & m_zipCoversGuard;
	std::mutex & m_zipImagesGuard;

	const std::unique_ptr<Zip> & m_zipCovers;
	const std::unique_ptr<Zip> & m_zipImages;

	std::vector<DataItem> m_covers;
	std::vector<DataItem> m_images;

	std::thread m_thread;
};

QString GetImagesFolder(const QDir & dir, const QString & type)
{
	const QFileInfo fileInfo(dir.path());
	return QString("%1/%2/%3.zip").arg(fileInfo.dir().path(), type, fileInfo.fileName());
}

class FileProcessor
{
public:
	FileProcessor(const Settings & settings, const int totalFiles, std::condition_variable & queueCondition, std::mutex & queueGuard, const int poolSize)
		: m_queueCondition { queueCondition }
		, m_queueGuard { queueGuard }
		, m_zipCovers { settings.covers ? std::make_unique<Zip>(GetImagesFolder(settings.dstDir, Global::COVERS), Zip::Format::Zip) : std::unique_ptr<Zip>{} }
		, m_zipImages { settings.images ? std::make_unique<Zip>(GetImagesFolder(settings.dstDir, Global::IMAGES), Zip::Format::Zip) : std::unique_ptr<Zip>{} }
	{
		for (int i = 0; i < poolSize; ++i)
			m_workers.push_back(std::make_unique<Worker>
				(
					  settings
					, totalFiles
					, m_queueCondition
					, m_queueGuard
					, m_queue
					, m_fileSystemGuard
					, m_hasError
					, m_queueSize
					, m_filesCount
					, m_zipCoversGuard
					, m_zipImagesGuard
					, m_zipCovers
					, m_zipImages
				)
			);
	}

public:
	int GetQueueSize() const
	{
		return m_queueSize;
	}

	void Enqueue(QString file, QByteArray data)
	{
		std::unique_lock lock(m_queueGuard);
		m_queue.emplace(std::move(file), std::move(data));
		++m_queueSize;
		m_queueCondition.notify_all();
	}

	bool HasError() const
	{
		return m_hasError;
	}

	void Wait()
	{
		m_workers.clear();
	}

private:
	std::condition_variable & m_queueCondition;
	std::mutex & m_queueGuard;
	std::atomic_bool m_hasError { false };
	std::queue<std::pair<QString, QByteArray>> m_queue;
	std::atomic_int m_queueSize { 0 }, m_filesCount { 0 };
	std::mutex m_fileSystemGuard;

	std::mutex m_zipCoversGuard;
	std::mutex m_zipImagesGuard;

	const std::unique_ptr<Zip> m_zipCovers;
	const std::unique_ptr<Zip> m_zipImages;

	std::vector<std::unique_ptr<Worker>> m_workers;
};

bool SevenZipIt(const QString & exe, const QString & folder, const int maxTreadCount)
{
	if (exe.isEmpty())
		return false;

	QProcess process;
	QEventLoop eventLoop;
	QStringList args = QStringList {}
		<< "a"
		<< "-mx9"
		<< "-sdel"
		<< "-m0=ppmd"
		<< "-ms=off"
		<< "-bt"
		<< QString("-mmt%1").arg(maxTreadCount)
		<< QString("%1.7z").arg(folder)
		<< QString("%1/*.fb2").arg(folder)
		;
		
	QObject::connect(&process, &QProcess::started, [&]
	{
		PLOGI << "\n" << QFileInfo(exe).fileName() << " " << args.join(" ");
	});
	QObject::connect(&process, &QProcess::finished, [&] (const int code, const QProcess::ExitStatus)
	{
		if (code == 0)
			PLOGI << QFileInfo(exe).fileName() << " finished successfully";
		else
			PLOGW << QFileInfo(exe).fileName() << " finished with " << code;
		eventLoop.exit(code);
	});
	QObject::connect(&process, &QProcess::readyReadStandardError, [&]
	{
		PLOGI << process.readAllStandardError();
	});
	QObject::connect(&process, &QProcess::readyReadStandardOutput, [&]
	{
		PLOGW << process.readAllStandardOutput();
	});

	process.start(exe, args, QIODevice::ReadOnly);
	if (eventLoop.exec() == 0)
		QDir().rmdir(folder);

	return false;
}

bool ProcessArchiveImpl(const QString & file, Settings settings)
{
	const QFileInfo fileInfo(file);
	settings.dstDir = QDir(settings.dstDir.filePath(fileInfo.completeBaseName()));
	[[maybe_unused]] const auto t = settings.dstDir.path();
	if (!settings.dstDir.exists() && !settings.dstDir.mkpath("."))
	{
		PLOGE << QString("Cannot create folder %1").arg(settings.dstDir.path());
		return true;
	}

	const Zip zip(file);
	auto fileList = zip.GetFileNameList();

	const auto maxThreadCount = std::min(std::max(settings.maxThreadCount, 1), static_cast<int>(fileList.size()));

	std::condition_variable queueCondition;
	std::mutex queueGuard;
	FileProcessor fileProcessor(settings, static_cast<int>(fileList.size()), queueCondition, queueGuard, maxThreadCount);

	while (!fileList.isEmpty())
	{
		if (fileProcessor.GetQueueSize() < maxThreadCount * 2)
		{
			auto & input = zip.Read(fileList.front());
			fileProcessor.Enqueue(std::move(fileList.front()), input.readAll());
			fileList.pop_front();
		}
		else
		{
			std::unique_lock lockStart(queueGuard);
			queueCondition.wait(lockStart, [&] ()
			{
				return fileProcessor.GetQueueSize() < maxThreadCount * 2;
			});
		}
	}

	for (int i = 0; i < maxThreadCount; ++i)
		fileProcessor.Enqueue({}, {});

	fileProcessor.Wait();

	bool hasError = false;
	hasError = fileProcessor.HasError() || hasError;
	hasError = SevenZipIt(settings.sevenZipPath, settings.dstDir.path(), settings.maxThreadCount) || hasError;

	return hasError;
}

bool ProcessArchive(const QString & file, const Settings & settings)
{
	try
	{
		PLOGI << QString("%1 processing").arg(file);
		if (ProcessArchiveImpl(file, settings))
		{
			PLOGE << QString("%1 processing failed").arg(file);
		}
		else
		{
			PLOGI << QString("%1 done").arg(file);
			return false;
		}
	}
	catch (const std::exception & ex)
	{
		PLOGE << QString("%1 processing failed: %2").arg(file).arg(ex.what());
	}
	catch (...)
	{
		PLOGE << QString("%1 processing failed").arg(file);
	}

	return true;
}

QStringList ProcessArchives(QStringList files, const Settings & settings)
{
	QStringList failed;
	for (auto && file : files)
		if (ProcessArchive(file, settings))
			failed << std::move(file);

	return failed;
}

QStringList ProcessWildCard(const QString & wildCard)
{
	const QFileInfo fileInfo(wildCard);
	const QDir dir(fileInfo.dir());
	const auto files = dir.entryList(QStringList() << fileInfo.fileName(), QDir::Files);
	QStringList result;
	result.reserve(files.size());
	std::ranges::transform(files, std::back_inserter(result), [&] (const auto & file)
	{
		return dir.filePath(file);
	});
	return result;
}

bool run(int argc, char * argv[])
{
	const QCoreApplication app(argc, argv);
	QCoreApplication::setApplicationName(APP_ID);
	QCoreApplication::setApplicationVersion(PRODUCT_VERSION);

	Settings settings {};

	QCommandLineParser parser;
	parser.setApplicationDescription(QString("%1 extracts images from *.fb2").arg(APP_ID));
	parser.addHelpOption();
	parser.addVersionOption();
	parser.addOptions({
		{ { QString(FOLDER[0]) , FOLDER  }                          , DESTINATION_FOLDER , FOLDER },
		{ { QString(QUALITY[0]), QUALITY_OPTION_NAME }              , COMPRESSION_QUALITY, QUALITY },
		{ { QString(THREADS[0]), MAX_THREAD_COUNT_OPTION_NAME }     , MAX_THREAD_COUNT   , QString(THREADS).arg(settings.maxThreadCount) },
		{ {QString(SEVEN_ZIP_OPTION_NAME[0]), SEVEN_ZIP_OPTION_NAME}, SEVEN_ZIP          , PATH},
		{ MAX_WIDTH_OPTION_NAME                                     , MAX_WIDTH          , QString(WIDTH).arg(settings.maxWidth) },
		{ MAX_HEIGHT_OPTION_NAME                                    , MAX_HEIGHT         , QString(HEIGHT).arg(settings.maxHeight) },
		{ NO_IMAGES_OPTION_NAME                                     , NO_IMAGES },
		{ COVERS_ONLY_OPTION_NAME                                   , COVERS_ONLY },
		});
	parser.process(app);

	const auto destinationFolder = parser.value(FOLDER);
	if (destinationFolder.isEmpty())
	{
		PLOGE << "Destination folder required";
		parser.showHelp(1);
	}
	settings.dstDir = QDir(destinationFolder);
	if (!settings.dstDir.exists() && !settings.dstDir.mkpath("."))
		throw std::ios_base::failure(QString("Cannot create folder %1").arg(destinationFolder).toStdString());

	settings.sevenZipPath = parser.value(SEVEN_ZIP_OPTION_NAME);

	const auto setIntegerValue = [&] (const char * name, int & value)
	{
		bool ok = false;
		if (const auto parsed = parser.value(name).toInt(&ok); ok)
			value = parsed;
	};

	setIntegerValue(MAX_WIDTH_OPTION_NAME, settings.maxWidth);
	setIntegerValue(MAX_HEIGHT_OPTION_NAME, settings.maxHeight);
	setIntegerValue(QUALITY_OPTION_NAME, settings.quality);
	setIntegerValue(MAX_THREAD_COUNT_OPTION_NAME, settings.maxThreadCount);

	if (parser.isSet(NO_IMAGES_OPTION_NAME))
		settings.covers = settings.images = false;
	else if (parser.isSet(COVERS_ONLY_OPTION_NAME))
		settings.images = false;

	if (settings.covers)
		if (const QDir dir(QString("%1/%2").arg(settings.dstDir.path(), Global::COVERS)); !dir.exists() && !dir.mkpath("."))
			throw std::ios_base::failure(QString("Cannot create folder %1").arg(dir.path()).toStdString());
	if (settings.images)
	if (const QDir dir(QString("%1/%2").arg(settings.dstDir.path(), Global::IMAGES)); !dir.exists() && !dir.mkpath("."))
		throw std::ios_base::failure(QString("Cannot create folder %1").arg(dir.path()).toStdString());

	QStringList files;
	for (const auto & wildCard : parser.positionalArguments())
		files << ProcessWildCard(wildCard);

	const auto failedArchives = ProcessArchives(std::move(files), settings);
	if (failedArchives.isEmpty())
		return false;

	PLOGE << "Failed:\n" << failedArchives.join("\n");
	return true;
}

}

int main(const int argc, char * argv[])
{
	plog::ConsoleAppender<plog::TxtFormatter> consoleAppender;
	Log::LogAppender logConsoleAppender(&consoleAppender);
	Log::LoggingInitializer logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, APP_ID).toStdWString());
	PLOGI << QString("%1 started").arg(APP_ID);

	try
	{
		if (run(argc, argv))
			PLOGI << QString("%1 failed").arg(APP_ID);
		else
			PLOGW << QString("%1 successfully finished").arg(APP_ID);
		return 0;
	}
	catch (const std::exception & ex)
	{
		PLOGE << QString("%1 failed: %2").arg(APP_ID).arg(ex.what());
	}
	catch (...)
	{
		PLOGE << QString("%1 failed").arg(APP_ID);
	}
	return 1;
}
