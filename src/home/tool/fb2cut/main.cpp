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
constexpr auto QUALITY_OPTION_NAME = "quality";

constexpr auto MAX_WIDTH = "Maximum image width";
constexpr auto MAX_HEIGHT = "Maximum image height";
constexpr auto COMPRESSION_QUALITY = "Compression quality [0, 100]";
constexpr auto DESTINATION_FOLDER = "Destination folder";

constexpr auto WIDTH = "width";
constexpr auto HEIGHT = "height";
constexpr auto QUALITY = "quality [-1]";
constexpr auto FOLDER = "folder";

struct Settings
{
	int maxWidth { std::numeric_limits<int>::max() };
	int maxHeight{ std::numeric_limits<int>::max() };
	int quality { -1 };
	QDir dstDir;
};

class FileProcessor
{
public:
	FileProcessor(const Settings & settings, const int totalFiles, std::condition_variable & queueCondition, std::mutex & queueGuard, const int poolSize)
		: m_settings(settings)
		, m_totalFiles(totalFiles)
		, m_queueCondition { queueCondition }
		, m_queueGuard { queueGuard }
	{
		for (int i = 0; i < poolSize; ++i)
			m_threads.push_back(std::make_unique<std::thread>(&FileProcessor::Process, this));
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

	void Wait() const
	{
		for (auto & thread : m_threads)
			thread->join();
	}

	const std::vector<std::pair<QString, QByteArray>>& GetCovers() const noexcept
	{
		return m_covers;
	}

	const std::vector<std::pair<QString, QByteArray>> & GetImages() const noexcept
	{
		return m_images;
	}

	const std::vector<std::pair<QString, QByteArray>> & GetErrors() const noexcept
	{
		return m_errors;
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
	}
	bool ProcessFile(const QString & inputFilePath, QByteArray& inputFileBody)
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
			QBuffer buffer(&body);
			buffer.open(QBuffer::ReadOnly);

			auto imageFile = isCover
				? QString("%1").arg(fileInfo.completeBaseName())
				: QString("%1/%2").arg(fileInfo.completeBaseName()).arg(name);

			QImageReader imageReader(&buffer);
			auto image = imageReader.read();
			if (image.isNull())
				return AddError(imageFile, std::move(body), QString("Incorrect image %1.%2: %3").arg(name).arg(imageFile).arg(imageReader.errorString()));

			if (image.width() > m_settings.maxWidth || image.height() > m_settings.maxHeight)
				image = image.scaled(m_settings.maxWidth, m_settings.maxHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);

			QByteArray imageBody;
			{
				QBuffer imageOutput(&imageBody);

				if (!image.save(&imageOutput, "jpeg", m_settings.quality))
					return AddError(imageFile, std::move(body), QString("Cannot compress image %1.%2").arg(name).arg(imageFile));
			}

			std::scoped_lock resultLock(isCover ? m_coversGuard : m_imagesGuard);
			(isCover ? m_covers : m_images).emplace_back(std::move(imageFile), std::move(imageBody));
		};

		Fb2Parser(inputFilePath, input, output, std::move(binaryCallback)).Parse();

		return bodyOutput;
	}

	void AddError(QString file, QByteArray body, const QString & errorText)
	{
		PLOGW << errorText;
		std::scoped_lock resultLock(m_errorsGuard);
		m_errors.emplace_back(std::move(file), std::move(body));
		m_hasError = true;
	}

private:
	const Settings & m_settings;
	const int m_totalFiles;
	std::condition_variable & m_queueCondition;
	std::mutex & m_queueGuard;
	std::atomic_bool m_hasError { false };
	std::queue<std::pair<QString, QByteArray>> m_queue;
	std::vector<std::pair<QString, QByteArray>> m_covers;
	std::vector<std::pair<QString, QByteArray>> m_images;
	std::vector<std::pair<QString, QByteArray>> m_errors;
	std::atomic_int m_queueSize { 0 }, m_filesCount { 0 };
	std::vector<std::unique_ptr<std::thread>> m_threads;
	std::mutex m_fileSystemGuard, m_coversGuard, m_imagesGuard, m_errorsGuard;
};

bool WriteImages(const QString & file, const std::vector<std::pair<QString, QByteArray>> &images)
{
	Zip zip(file, Zip::Format::Zip);
	return !std::ranges::all_of(images, [&] (const auto & item)
	{
		return zip.Write(item.first).write(item.second) == item.second.size();
	});
}

void WriteErrors(const QDir & dir, const std::vector<std::pair<QString, QByteArray>> & images)
{
	for (const auto& [name, body] : images)
	{
		const auto filePath = dir.filePath(QString("error/%1.bad").arg(name));
		const QDir imgDir = QFileInfo(filePath).dir();
		if (!imgDir.exists() && !imgDir.mkpath("."))
		{
			PLOGE << QString("Cannot create folder %1").arg(imgDir.path());
			continue;
		}

		QFile file(filePath);
		if (!file.open(QIODevice::WriteOnly))
		{
			PLOGE << QString("Cannot write to %1").arg(filePath);
			continue;
		}

		if (file.write(body) != body.size())
			PLOGE << QString("%1 written with errors").arg(filePath);
	}
}

bool ProcessArchiveImpl(const QString & file, Settings settings)
{
	const QFileInfo fileInfo(file);
	settings.dstDir = QDir(settings.dstDir.filePath(fileInfo.completeBaseName()));
	if (!settings.dstDir.exists() && !settings.dstDir.mkpath("."))
	{
		PLOGE << QString("Cannot create folder %1").arg(settings.dstDir.path());
		return true;
	}

	const Zip zip(file);
	auto fileList = zip.GetFileNameList();

	const auto cpuCount = static_cast<int>(std::thread::hardware_concurrency());
	const auto maxThreadCount = std::min(std::max(cpuCount - 2, 1), static_cast<int>(fileList.size()));

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
	bool hasErrors = fileProcessor.HasError();

	WriteErrors(settings.dstDir, fileProcessor.GetErrors());

	hasErrors = WriteImages(QString("%1_covers.zip").arg(settings.dstDir.path()), fileProcessor.GetCovers()) || hasErrors;
	hasErrors = WriteImages(QString("%1_img.zip").arg(settings.dstDir.path()), fileProcessor.GetImages()) || hasErrors;

	return hasErrors;
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

QStringList ProcessArchives(QStringList files, const Settings& settings)
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

	QCommandLineParser parser;
	parser.setApplicationDescription(QString("%1 extracts images from *.fb2").arg(APP_ID));
	parser.addHelpOption();
	parser.addVersionOption();
	parser.addOptions({
		{ MAX_WIDTH_OPTION_NAME           , MAX_WIDTH         , WIDTH },
		{ MAX_HEIGHT_OPTION_NAME          , MAX_HEIGHT        , HEIGHT },
		{ { QString(QUALITY[0]), QUALITY_OPTION_NAME }, COMPRESSION_QUALITY           , QUALITY },
		{ { QString(FOLDER[0]) , FOLDER  }, DESTINATION_FOLDER, FOLDER },
		});
	parser.process(app);

	const auto setIntegerValue = [&] (const char * name, int& value)
	{
		bool ok = false;
		if (const auto parsed = parser.value(name).toInt(&ok); ok)
			value = parsed;
	};

	Settings settings{};
	setIntegerValue(MAX_WIDTH_OPTION_NAME, settings.maxWidth);
	setIntegerValue(MAX_HEIGHT_OPTION_NAME, settings.maxHeight);
	setIntegerValue(QUALITY_OPTION_NAME, settings.quality);

	const auto destinationFolder = parser.value(FOLDER);
	if (destinationFolder.isEmpty())
	{
		PLOGE << "Destination folder required";
		parser.showHelp(1);
	}

	settings.dstDir = QDir(destinationFolder);
	if (!settings.dstDir.exists() && !settings.dstDir.mkpath("."))
		throw std::ios_base::failure(QString("Cannot create folder %1").arg(destinationFolder).toStdString());

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
