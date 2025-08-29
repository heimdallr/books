#include <queue>
#include <ranges>

#include <QBuffer>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QPixmap>
#include <QSize>
#include <QStandardPaths>
#include <QString>

#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Util.h>

#include "fnd/ScopedCall.h"

#include "jxl/jxl.h"
#include "logging/LogAppender.h"
#include "logging/init.h"
#include "logic/shared/ImageRestore.h"
#include "util/LogConsoleFormatter.h"
#include "util/files.h"

#include "log.h"
#include "zip.h"

#include "config/version.h"

using namespace HomeCompa;

namespace
{

constexpr auto APP_ID = "flimager";

constexpr auto IMAGE_ARCHIVE_WILDCARD_OPTION_NAME = "archives";
constexpr auto QUALITY_OPTION_NAME = "quality";
constexpr auto MAX_SIZE_OPTION_NAME = "max-size";
constexpr auto MAX_WIDTH_OPTION_NAME = "max-width";
constexpr auto MAX_HEIGHT_OPTION_NAME = "max-height";
constexpr auto GRAYSCALE_OPTION_NAME = "grayscale";
constexpr auto MAX_THREAD_COUNT_OPTION_NAME = "threads";
constexpr auto FOLDER = "folder";
constexpr auto QUALITY = "quality [-1]";
constexpr auto SIZE = "size [INT_MAX]";
constexpr auto THREADS = "threads [%1]";
constexpr auto FORMAT = "format";

using Queue = std::queue<QString>;

struct Settings
{
	QDir outputDir;
	QString inputDir;
	QStringList inputFiles;
	QString format { JXL::FORMAT };
	int quality { -1 };
	bool grayScale { false };
	QSize size { std::numeric_limits<int>::max(), std::numeric_limits<int>::max() };
	size_t maxThreadCount { static_cast<size_t>(std::thread::hardware_concurrency()) };
	int totalImageCount { 0 };
};

QByteArray Encode(const QImage& image, const char* format, const int quality)
{
	QByteArray result;
	{
		QBuffer buffer(&result);
		image.save(&buffer, format, quality);
	}
	return result;
}

QByteArray EncodeJpeg(const QImage& image, const int quality)
{
	const bool hasAlpha = image.pixelFormat().alphaUsage() == QPixelFormat::UsesAlpha;
	return Encode(image, hasAlpha ? "png" : "jpeg", quality);
}

QByteArray EncodePng(const QImage& image, const int quality)
{
	return Encode(image, "png", quality);
}

class Worker
{
	NON_COPY_MOVABLE(Worker)

public:
	Worker(const Settings& settings, std::mutex& queueGuard, Queue& queue, std::atomic_bool& hasError, std::atomic_int& imageCount)
		: m_settings { settings }
		, m_queueGuard { queueGuard }
		, m_queue { queue }
		, m_hasError { hasError }
		, m_imageCount { imageCount }
		, m_thread { &Worker::Process, this }
		, m_encoder { settings.format == JXL::FORMAT ? &JXL::Encode
		              : settings.format == "PNG"     ? &EncodePng
		              : settings.format == "JPEG"    ? &EncodeJpeg
		                                             : &JXL::Encode }
	{
	}

	~Worker()
	{
		if (m_thread.joinable())
			m_thread.join();
	}

private:
	void Process() const
	{
		while (true)
		{
			QString archive;
			{
				std::unique_lock queueLock(m_queueGuard);
				if (m_queue.empty())
					return;

				archive = std::move(m_queue.front());
				m_queue.pop();
			}

			PLOGI << "process " << archive << " started";

			try
			{
				ProcessArchive(archive);
				PLOGI << "process " << archive << " finished";
			}
			catch (const std::exception& ex)
			{
				m_hasError = true;
				PLOGE << "process " << archive << " failed: " << ex.what();
			}
		}
	}

	void ProcessArchive(const QString& fileName) const
	{
		const auto outputFileName = m_settings.outputDir.filePath(QString(fileName).remove(':'));
		if (QFile::exists(outputFileName) && !QFile::remove(outputFileName))
			throw std::ios_base::failure(QString("Cannot remove %1").arg(outputFileName).toStdString());

		auto recoded = Recode(m_settings.inputDir + fileName);
		Write(outputFileName, std::move(recoded));
	}

	void Write(const QString& outputFileName, std::vector<std::tuple<QString, QByteArray, QDateTime>> data) const
	{
		PLOGI << "archive " << data.size() << " images to " << outputFileName;

		if (const auto dstDir = QFileInfo(outputFileName).dir(); !dstDir.exists() && !dstDir.mkpath("."))
			throw std::ios_base::failure(QString("Cannot create %1").arg(dstDir.path()).toStdString());

		auto zipFiles = Zip::CreateZipFileController();
		for (auto&& [fileName, body, time] : data)
			zipFiles->AddFile(std::move(fileName), std::move(body), std::move(time));

		Zip zip(outputFileName, Zip::Format::Zip);
		zip.SetProperty(Zip::PropertyId::CompressionLevel, QVariant::fromValue(Zip::CompressionLevel::Ultra));
		zip.SetProperty(Zip::PropertyId::ThreadsCount, m_settings.maxThreadCount);
		zip.Write(std::move(zipFiles));

		PLOGI << "archive " << outputFileName << " done";
	}

	std::vector<std::tuple<QString, QByteArray, QDateTime>> Recode(const QString& fileName) const
	{
		const QFileInfo fileInfo(fileName);

		std::vector<std::tuple<QString, QByteArray, QDateTime>> result;
		const Zip zip(fileInfo.filePath());
		for (auto imageFile : zip.GetFileNameList())
		{
			const ScopedCall fileCountGuard(
				[&, percents = m_imageCount * 100 / m_settings.totalImageCount]()
				{
					int imageCount = ++m_imageCount;
					const auto currentPercents = imageCount * 100 / m_settings.totalImageCount;
					if (percents != currentPercents || imageCount % 100 == 0)
						PLOGV << QString("%1, %2 (%3) %4%").arg(fileName.last(fileName.length() - m_settings.inputDir.length())).arg(imageCount).arg(m_settings.totalImageCount).arg(currentPercents);
				});

			const auto imageBody = zip.Read(imageFile)->GetStream().readAll();
			if (auto recoded = Recode(imageBody); !recoded.isEmpty())
			{
				auto time = zip.GetFileTime(imageFile);
				result.emplace_back(std::move(imageFile), std::move(recoded), std::move(time));
			}
			else
			{
				m_hasError = true;
				PLOGE << "Cannot recode " << fileInfo.fileName() << "/" << imageFile;
			}
		}

		return result;
	}

	QByteArray Recode(const QByteArray& src) const
	{
		auto image = Flibrary::Decode(src).toImage();
		if (image.isNull())
			return {};

		if (m_settings.grayScale)
			image.convertTo(QImage::Format::Format_Grayscale8);

		if (image.pixelFormat().colorModel() != QPixelFormat::Grayscale)
			image = Flibrary::HasAlpha(image, src.constData());

		const bool hasAlpha = image.pixelFormat().alphaUsage() == QPixelFormat::UsesAlpha;
		if (image.width() > m_settings.size.width() || image.height() > m_settings.size.height())
			image = image.scaled(m_settings.size.width(), m_settings.size.height(), Qt::KeepAspectRatio, hasAlpha ? Qt::FastTransformation : Qt::SmoothTransformation);

		assert(m_encoder);
		return m_encoder(image, m_settings.quality);
	}

private:
	const Settings& m_settings;
	std::mutex& m_queueGuard;
	Queue& m_queue;
	std::atomic_bool& m_hasError;
	std::atomic_int& m_imageCount;
	std::thread m_thread;
	using Encoder = QByteArray (*)(const QImage& image, int quality);
	Encoder m_encoder { nullptr };
};

bool ProcessArchives(const Settings& settings)
{
	std::atomic_bool hasError { false };

	{
		std::mutex queueGuard;
		Queue queue;
		std::atomic_int imageCount { 0 };

		for (const auto& archive : settings.inputFiles)
			queue.push(archive);

		std::vector<std::unique_ptr<Worker>> workers;
		workers.reserve(settings.maxThreadCount);
		for (size_t i = 0; i < settings.maxThreadCount; ++i)
			workers.emplace_back(std::make_unique<Worker>(settings, queueGuard, queue, hasError, imageCount));
	}

	return hasError;
}

void GetArchives(Settings& settings, const QStringList& wildCards)
{
	for (const auto& wildCard : wildCards)
		settings.inputFiles << Util::ResolveWildcard(wildCard);

	settings.inputDir = QDir::fromNativeSeparators(QFileInfo(settings.inputFiles.front()).dir().path() + QDir::separator());
	for (const auto& inputFile : settings.inputFiles | std::views::drop(1))
	{
		while (!settings.inputDir.isEmpty() && !inputFile.startsWith(settings.inputDir, Qt::CaseInsensitive))
		{
			QDir inputDir(settings.inputDir);
			if (inputDir.cdUp())
				settings.inputDir = QDir::fromNativeSeparators(inputDir.path() + QDir::separator());
			else
				settings.inputDir.clear();
		}

		if (settings.inputDir.isEmpty())
			break;
	}

	if (!settings.inputDir.isEmpty())
		std::ranges::transform(settings.inputFiles, settings.inputFiles.begin(), [n = settings.inputDir.length()](const QString& item) { return item.last(item.length() - n); });

	PLOGD << "Total image count calculation";
	settings.totalImageCount = std::accumulate(settings.inputFiles.cbegin(),
	                                           settings.inputFiles.cend(),
	                                           settings.totalImageCount,
	                                           [&](const auto init, const QString& file)
	                                           {
												   const Zip zip(settings.inputDir + file);
												   return init + zip.GetFileNameList().size();
											   });
	PLOGI << "Total image count: " << settings.totalImageCount;
}

Settings ProcessCommandLine(const QCoreApplication& app)
{
	Settings settings;

	QCommandLineParser parser;
	parser.setApplicationDescription(QString("%1 recodes images").arg(APP_ID));
	parser.addHelpOption();
	parser.addVersionOption();
	parser.addPositionalArgument(IMAGE_ARCHIVE_WILDCARD_OPTION_NAME, "Input image archive files (required)");
	parser.addOptions({
		{ { "o", FOLDER }, "Output folder (required)", FOLDER },
		{ { "f", FORMAT }, "Output images format [JXL | JPEG | PNG]", QString("%1 [%2]").arg(FORMAT, "JXL") },
		{ { "q", QUALITY_OPTION_NAME }, "Compression quality [0, 100] or -1 for default compression quality", QUALITY },
		{ MAX_WIDTH_OPTION_NAME, "Maximum images width", SIZE },
		{ MAX_HEIGHT_OPTION_NAME, "Maximum images height", SIZE },
		{ { "s", MAX_SIZE_OPTION_NAME }, "Maximum image size", SIZE },
		{ { "g", GRAYSCALE_OPTION_NAME }, "Convert all images to grayscale" },
		{ { "t", MAX_THREAD_COUNT_OPTION_NAME }, "Maximum number of CPU threads", QString(THREADS).arg(settings.maxThreadCount) },
	});

	parser.process(app);

	bool ok = false;

	if (const auto& positionalArguments = parser.positionalArguments(); !positionalArguments.isEmpty())
	{
		GetArchives(settings, parser.positionalArguments());
	}
	else
	{
		PLOGE << "Specifying input archives is mandatory";
		parser.showHelp();
	}

	if (parser.isSet(FOLDER))
	{
		settings.outputDir = QDir { parser.value(FOLDER) };
	}
	else
	{
		PLOGE << "Specifying output folder is mandatory";
		parser.showHelp();
	}

	settings.grayScale = parser.isSet(GRAYSCALE_OPTION_NAME);

	if (auto value = parser.value(FORMAT); !value.isEmpty())
		settings.format = std::move(value);

	if (const auto value = parser.value(MAX_SIZE_OPTION_NAME).toInt(&ok); ok)
		settings.size = { value, value };

	if (const auto value = parser.value(MAX_WIDTH_OPTION_NAME).toInt(&ok); ok)
		settings.size.setWidth(value);

	if (const auto value = parser.value(MAX_HEIGHT_OPTION_NAME).toInt(&ok); ok)
		settings.size.setHeight(value);

	if (const auto value = parser.value(QUALITY_OPTION_NAME).toInt(&ok); ok)
		settings.quality = value;

	if (const auto value = parser.value(MAX_THREAD_COUNT_OPTION_NAME).toULongLong(&ok); ok)
		settings.maxThreadCount = value;

	if (QCoreApplication::arguments().size() < 2)
		parser.showHelp();

	if (!settings.outputDir.exists() && !settings.outputDir.mkpath("."))
		throw std::ios_base::failure(QString("Cannot create folder %1").arg(settings.outputDir.path()).toStdString());

	return settings;
}

bool run(int argc, char* argv[])
{
	const QGuiApplication app(argc, argv); //-V821
	QCoreApplication::setApplicationName(APP_ID);
	QCoreApplication::setApplicationVersion(PRODUCT_VERSION);
	return ProcessArchives(ProcessCommandLine(app));
}

} // namespace

int main(const int argc, char* argv[])
{
	Log::LoggingInitializer logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, APP_ID).toStdWString());
	plog::ConsoleAppender<Util::LogConsoleFormatter> consoleAppender;
	Log::LogAppender logConsoleAppender(&consoleAppender);
	PLOGI << QString("%1 started").arg(APP_ID);

	try
	{
		if (run(argc, argv))
			PLOGW << QString("%1 finished with errors").arg(APP_ID);
		else
			PLOGI << QString("%1 successfully finished").arg(APP_ID);
		return 0;
	}
	catch (const std::exception& ex)
	{
		PLOGE << QString("%1 failed: %2").arg(APP_ID).arg(ex.what());
	}
	catch (...)
	{
		PLOGE << QString("%1 failed").arg(APP_ID);
	}
	return 1;
}
