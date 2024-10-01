#include <queue>
#include <ranges>

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QPixmap>
#include <QStandardPaths>
#include <QBuffer>

#include <plog/Log.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ConsoleAppender.h>

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

constexpr auto MAX_WIDTH = "Maximum image width";
constexpr auto MAX_HEIGHT = "Maximum image height";
constexpr auto WIDTH = "width";
constexpr auto HEIGHT = "height";
constexpr auto DESTINATION_FOLDER = "Destination folder";
constexpr auto FOLDER = "folder";

constexpr auto COVER = "cover";

class FileProcessor
{
public:
	FileProcessor(const QDir & dstDir, const int maxWidth, const int maxHeight, std::condition_variable & queueCondition, std::mutex & queueGuard, const int poolSize)
		: m_dstDir(dstDir)
		, m_maxWidth { maxWidth }
		, m_maxHeight { maxHeight }
		, m_queueCondition { queueCondition }
		, m_queueGuard { queueGuard }
	{
		for (int i = 0; i < poolSize; ++i)
			m_threads.push_back(std::make_unique<std::thread>(&FileProcessor::Process, this));
	}

public:
	int GetQueueSize() const
	{
		return m_count;
	}

	void Enqueue(QString file, QByteArray data)
	{
		std::unique_lock lock(m_queueGuard);
		m_queue.emplace(std::move(file), std::move(data));
		++m_count;
		m_queueCondition.notify_all();
	}

	bool HasError() const
	{
		return m_hasError;
	}

	void Wait() const
	{
		for (auto & thread : m_threads)
			thread->join();
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
				--m_count;
				m_queueCondition.notify_all();
				name = std::move(f);
				body = std::move(d);
			}

			if (body.isEmpty())
				break;

			m_hasError = ProcessFile(name, body) || m_hasError;
		}
	}
	bool ProcessFile(const QString & inputFilePath, QByteArray& inputFileBody)
	{
		QBuffer input(&inputFileBody);
		input.open(QIODevice::ReadOnly);

		const QFileInfo fileInfo(inputFilePath);
		const auto outputFilePath = m_dstDir.filePath(fileInfo.fileName());

		const auto bodyOutput = ParseFile(inputFilePath, input);
		if (bodyOutput.isEmpty())
			return true;

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

		bool hasError = false;
		auto binaryCallback = [&] (const QString & name, const bool isCover, QByteArray body)
		{
			QBuffer buffer(&body);
			buffer.open(QBuffer::ReadOnly);

			QImageReader imageReader(&buffer);
			auto image = imageReader.read();
			if (image.isNull())
			{
				PLOGW << QString("Incorrect image %1: %2").arg(name).arg(imageReader.errorString());
				hasError = true;
				return true;
			}
			image = image.scaled(m_maxWidth, m_maxHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);

			auto imageFile = isCover
				? QString("%1/%2").arg(COVER).arg(fileInfo.completeBaseName())
				: QString("%1/%2").arg(fileInfo.completeBaseName()).arg(name);

			QByteArray imageBody;
			{
				QBuffer imageOutput(&imageBody);

				if (!image.save(&imageOutput, "jpeg", 20))
				{
					PLOGW << QString("Cannot compress image %1.%2").arg(name).arg(imageFile);
					hasError = true;
				}
			}

			std::scoped_lock resultLock(m_resultGuard);
			m_result.emplace_back(std::move(imageFile), std::move(imageBody));
			return true;
		};

		Fb2Parser(inputFilePath, input, output, std::move(binaryCallback)).Parse();

		return hasError ? QByteArray {} : bodyOutput;
	}

private:
	const QDir & m_dstDir;
	const int m_maxWidth, m_maxHeight;
	std::condition_variable & m_queueCondition;
	std::mutex & m_queueGuard;
	std::atomic_bool m_hasError { false };
	std::queue<std::pair<QString, QByteArray>> m_queue;
	std::vector<std::pair<QString, QByteArray>> m_result;
	std::atomic_int m_count { 0 };
	std::vector<std::unique_ptr<std::thread>> m_threads;
	std::mutex m_fileSystemGuard, m_resultGuard;
};

bool ProcessArchiveImpl(const QString & file, const QDir & dstDir, const int maxWidth, const int maxHeight)
{
	const QFileInfo fileInfo(file);
	const QDir archiveDir(dstDir.filePath(fileInfo.completeBaseName()));
	if (!archiveDir.exists() && !archiveDir.mkpath("."))
	{
		PLOGE << QString("Cannot create folder %1").arg(archiveDir.dirName());
		return true;
	}

	const Zip zip(file);
	auto fileList = zip.GetFileNameList();

	const auto cpuCount = static_cast<int>(std::thread::hardware_concurrency());
	const auto maxThreadCount = std::min(std::max(cpuCount - 2, 1), static_cast<int>(fileList.size()));

	std::condition_variable queueCondition;
	std::mutex queueGuard;
	FileProcessor fileProcessor(archiveDir, maxWidth, maxHeight, queueCondition, queueGuard, maxThreadCount);

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

	return fileProcessor.HasError();
}

bool ProcessArchive(const QString & file, const QDir & dstDir, const int maxWidth, const int maxHeight)
{
	try
	{
		PLOGI << QString("%1 processing").arg(file);
		if (ProcessArchiveImpl(file, dstDir, maxWidth, maxHeight))
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

QStringList ProcessArchives(QStringList files, const QDir & dstDir, const int maxWidth, const int maxHeight)
{
	QStringList failed;
	for (auto && file : files)
		if (ProcessArchive(file, dstDir, maxWidth, maxHeight))
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

	QCommandLineParser parser;
	parser.setApplicationDescription(QString("%1 extracts images from *.fb2").arg(APP_ID));
	parser.addHelpOption();
	parser.addVersionOption();
	parser.addOptions({
		{ MAX_WIDTH_OPTION_NAME         , MAX_WIDTH         , WIDTH },
		{ MAX_HEIGHT_OPTION_NAME        , MAX_HEIGHT        , HEIGHT },
		{ { QString(FOLDER[0]), FOLDER }, DESTINATION_FOLDER, FOLDER },
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
		PLOGE << "Destination folder required";
		parser.showHelp(1);
	}

	const QDir dstDir(destinationFolder);
	if (!dstDir.exists() && !dstDir.mkpath("."))
		throw std::ios_base::failure(QString("Cannot create folder %1").arg(destinationFolder).toStdString());

	QStringList files;
	for (const auto & wildCard : parser.positionalArguments())
		files << ProcessWildCard(wildCard);

	const auto failedArchives = ProcessArchives(std::move(files), dstDir, maxWidth, maxHeight);
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
