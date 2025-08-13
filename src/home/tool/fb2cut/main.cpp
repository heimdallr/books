#include <QCryptographicHash>

#include <queue>
#include <ranges>

#include <QApplication>
#include <QBuffer>
#include <QCommandLineParser>
#include <QImageReader>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTranslator>

#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Record.h>
#include <plog/Util.h>

#include "fnd/FindPair.h"
#include "fnd/NonCopyMovable.h"
#include "fnd/ScopedCall.h"

#include "GuiUtil/interface/IUiFactory.h"
#include "Hypodermic/Hypodermic.h"
#include "jxl/jxl.h"
#include "logging/LogAppender.h"
#include "logging/init.h"
#include "logic/shared/ImageRestore.h"
#include "util/ISettings.h"
#include "util/LogConsoleFormatter.h"
#include "util/files.h"
#include "util/localization.h"
#include "util/xml/Initializer.h"
#include "util/xml/Validator.h"

#include "Fb2Parser.h"
#include "ImageSettingsWidget.h"
#include "MainWindow.h"
#include "di_app.h"
#include "log.h"
#include "settings.h"
#include "zip.h"

#include "config/version.h"

using namespace HomeCompa;
using namespace fb2cut;

namespace
{

constexpr auto APP_ID = "fb2cut";
constexpr auto MAX_SIZE_OPTION_NAME = "max-size";
constexpr auto MAX_COVER_SIZE_OPTION_NAME = "max-cover-size";
constexpr auto MAX_IMAGE_SIZE_OPTION_NAME = "max-image-size";

constexpr auto QUALITY_OPTION_NAME = "quality";
constexpr auto COVER_QUALITY_OPTION_NAME = "cover-quality";
constexpr auto IMAGE_QUALITY_OPTION_NAME = "image-quality";

constexpr auto GRAYSCALE_OPTION_NAME = "grayscale";
constexpr auto COVER_GRAYSCALE_OPTION_NAME = "cover-grayscale";
constexpr auto IMAGE_GRAYSCALE_OPTION_NAME = "image-grayscale";
constexpr auto ARCHIVER_OPTION_NAME = "archiver";
constexpr auto ARCHIVER_COMMANDLINE_OPTION_NAME = "archiver-options";

constexpr auto MAX_THREAD_COUNT_OPTION_NAME = "threads";
constexpr auto NO_ARCHIVE_FB2_OPTION_NAME = "no-archive-fb2";
constexpr auto NO_FB2_OPTION_NAME = "no-fb2";
constexpr auto NO_IMAGES_OPTION_NAME = "no-images";
constexpr auto COVERS_ONLY_OPTION_NAME = "covers-only";
constexpr auto FFMPEG_OPTION_NAME = "ffmpeg";
constexpr auto MIN_IMAGE_FILE_SIZE_OPTION_NAME = "min-image-file-size";
constexpr auto FORMAT = "format";
constexpr auto IMAGE_STATISTICS = "image-statistics";

constexpr auto GUI_MODE_OPTION_NAME = "gui";

constexpr auto QUALITY = "quality [-1]";
constexpr auto THREADS = "threads [%1]";
constexpr auto FOLDER = "folder";
constexpr auto PATH = "path";
constexpr auto COMMANDLINE = "list of options";
constexpr auto SIZE = "size [INT_MAX,INT_MAX]";

constexpr auto COVER_ID = "COVER_COVER_COVER_COVER123";

struct DataItem
{
	QString fileName;
	QByteArray body;
	QDateTime dateTime;
};

using DataItems = std::queue<DataItem>;

struct ImageStatisticsItem
{
	enum class PixelSchema
	{
		Normal,
		GrayScale,
		Alpha,
	};
	QString folder;
	QString fileName;
	QString ordNum;
	size_t size;
	int width;
	int height;
	PixelSchema schema;
	QString md5;
};

using ImageStatistics = std::vector<ImageStatisticsItem>;

void WriteError(const QDir& dir, std::mutex& guard, const QString& name, const QString& ext, const QByteArray& body)
{
	std::scoped_lock lock(guard);

	const auto filePath = dir.filePath(QString("error/%1.%2").arg(name, !ext.isEmpty() ? ext : "bad"));
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

std::pair<QImage, QString> ToImage(QByteArray& body)
{
	QBuffer buffer(&body);
	buffer.open(QBuffer::ReadOnly);
	QImageReader imageReader(&buffer);
	std::pair<QImage, QString> result { imageReader.read(), {} };
	if (result.first.isNull())
		result.second = imageReader.errorString();

	return result;
}

QString Validate(const Util::XmlValidator& validator, QByteArray& body)
{
	QBuffer buffer(&body);
	buffer.open(QIODevice::ReadOnly);
	return validator.Validate(buffer);
}

QByteArray FixInputFile(QByteArray inputFileBody)
{
	auto str = [&inputFileBody]
	{
		if (inputFileBody.startsWith("\xff\xfe") || inputFileBody.startsWith("\xfe\xff"))
		{
			inputFileBody.append("\0\0", 2);
			auto result = QString::fromUtf16(std::bit_cast<const char16_t*>(inputFileBody.constData()));
			return result.replace(QRegularExpression(R"(encoding=".*?")"), R"(encoding="UTF-8")");
		}

		static constexpr const char* win1251[] {
			R"(<?xml version="1.0" encoding="windows-)",
			R"(<?xml version="1.0" encoding="Windows-)",
		};

		if (std::ranges::any_of(win1251, [&](const auto* enc) { return inputFileBody.startsWith(enc); }))
		{
			auto result = QString::fromLocal8Bit(inputFileBody, inputFileBody.size());
			return result.replace(QRegularExpression(R"(encoding=".*?")"), R"(encoding="UTF-8")");
		}

		if (inputFileBody.startsWith(R"(<?xml version="1.0" encoding="iso-8859-15"?>)"))
		{
			auto encoded = QString::fromLocal8Bit(inputFileBody, inputFileBody.size());
			encoded.replace(R"(encoding="iso-8859-15")", R"(encoding="UTF-8")", Qt::CaseInsensitive);
			QString result;
			for (qsizetype i = 0, sz = encoded.length(); i < sz; ++i)
			{
				if (encoded[i] != '&')
				{
					result.append(encoded[i]);
					continue;
				}

				if (encoded[i + 1] != '#' || encoded[i + 6] != ';')
					continue;

				auto ref = QStringView { encoded.data() + i + 2, 4 };
				[[maybe_unused]] bool ok = false;
				const auto code = ref.toInt(&ok);
				assert(ok);
				result.append(QChar { code });

				i += 6;
			}
			return result;
		}

		return QString::fromUtf8(inputFileBody);
	}();

	const auto addEnd = [&](const qsizetype index)
	{
		str.resize(index);
		str.append("</FictionBook>");
	};

	str.replace(QRegularExpression(R"(<([^a-z//]+?)>)", QRegularExpression::CaseInsensitiveOption), "&lt;/1&gt;");
	if (const auto endIndex = str.indexOf("</FictionBook>"); endIndex > 0)
	{
		str.resize(endIndex + 14);
	}
	else if (const auto binaryIndex = str.lastIndexOf("<binary"); binaryIndex > 0)
	{
		addEnd(binaryIndex);
	}
	else if (const auto bodyIndex = str.lastIndexOf("</body>"); bodyIndex > 0)
	{
		addEnd(bodyIndex + 7);
	}
	return str.toUtf8();
}

class Worker
{
	NON_COPY_MOVABLE(Worker)

public:
	class IClient // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~IClient() = default;
		virtual void OnWorkFinished(std::vector<DataItem> covers, std::vector<DataItem> images, ImageStatistics imageStatistics) = 0;
	};

public:
	Worker(const Settings& settings,
	       QString folder,
	       std::condition_variable& queueCondition,
	       std::mutex& queueGuard,
	       DataItems& queue,
	       std::mutex& fileSystemGuard,
	       std::atomic_bool& hasError,
	       std::atomic_int& queueSize,
	       std::atomic_int& fileCount,
	       IClient& client)
		: m_settings { settings }
		, m_folder { std::move(folder) }
		, m_queueCondition { queueCondition }
		, m_queueGuard { queueGuard }
		, m_queue { queue }
		, m_fileSystemGuard { fileSystemGuard }
		, m_hasError { hasError }
		, m_queueSize { queueSize }
		, m_fileCount { fileCount }
		, m_client { client }
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
			QDateTime dateTime;
			{
				std::unique_lock queueLock(m_queueGuard);
				m_queueCondition.wait(queueLock, [&]() { return !m_queue.empty(); });

				auto [f, d, t] = std::move(m_queue.front());
				m_queue.pop();
				--m_queueSize;
				m_queueCondition.notify_all();
				name = std::move(f);
				body = std::move(d);
				dateTime = std::move(t);
			}

			if (body.isEmpty())
				break;

			m_hasError = ProcessFile(name, body, dateTime) || m_hasError;
		}

		m_client.OnWorkFinished(std::move(m_covers), std::move(m_images), std::move(m_imageStatistics));
	}

	bool ProcessFile(const QString& inputFilePath, const QByteArray& inputFileBody, const QDateTime& dateTime)
	{
		auto fixedInputFileBody = FixInputFile(inputFileBody);

		QBuffer input(&fixedInputFileBody);
		input.open(QIODevice::ReadOnly);

		const QFileInfo fileInfo(inputFilePath);
		const auto outputFilePath = m_settings.dstDir.filePath(fileInfo.fileName());

		auto bodyOutput = ParseFile(inputFilePath, input, dateTime);
		++m_fileCount;
		PLOGV << QString("%1, %2 (%3) %4%").arg(inputFilePath).arg(m_fileCount.load()).arg(m_settings.totalFileCount).arg(m_fileCount * 100 / m_settings.totalFileCount);

		if (bodyOutput.isEmpty())
			return AddError("fb2", fileInfo.completeBaseName(), inputFileBody, QString("Cannot parse %1").arg(outputFilePath), "fb2", false), true;

		//		AddError("fb2", fileInfo.completeBaseName(), fixedInputFileBody, QString("Validation %1 failed: %2").arg(outputFilePath, ""), "fb2", false);
		//		AddError("fb2", fileInfo.completeBaseName(), bodyOutput, QString("Validation %1 failed: %2").arg(outputFilePath, ""), "fb2", false);
		if (const auto errorText = Validate(m_validator, bodyOutput); !errorText.isEmpty())
			return AddError("fb2", fileInfo.completeBaseName(), inputFileBody, QString("Validation %1 failed: %2").arg(outputFilePath, errorText), "fb2", false), true;

		if (!m_settings.saveFb2)
			return false;

		std::scoped_lock fileSystemLock(m_fileSystemGuard);
		QFile bodyFile(outputFilePath);
		if (!bodyFile.open(QIODevice::WriteOnly))
		{
			PLOGW << QString("Cannot write body to %1").arg(outputFilePath);
			return true;
		}

		if (bodyFile.write(bodyOutput) != bodyOutput.size())
			return true;

		return !bodyFile.setFileTime(dateTime, QFile::FileTime::FileBirthTime);
	}

	QByteArray ParseFile(const QString& inputFilePath, QIODevice& input, const QDateTime& dateTime)
	{
		const QFileInfo fileInfo(inputFilePath);
		const auto completeFileName = fileInfo.completeBaseName();

		QByteArray bodyOutput;
		QBuffer output(&bodyOutput);
		output.open(QIODevice::WriteOnly);

		const auto encode = [this](const ImageSettings& settings, const QString& fileName, const QImage& image, const QByteArray& body) -> QByteArray
		{
			if (fileName.isEmpty())
				return {};

			if (auto bytes = JXL::Encode(image, settings.quality); !bytes.isEmpty())
				return bytes;

			(void)AddError(settings.type, fileName, body, QString("Cannot compress %1 %2").arg(settings.type).arg(fileName), {}, false);
			return {};
		};

		std::unordered_map<QString, int> uniqueData;
		std::unordered_map<QString, int> idToNum;

		auto binaryCallback = [&](QString&& name, const bool isCover, QByteArray body)
		{
			const auto& settings = isCover ? m_settings.cover : m_settings.image;
			if (!settings.save)
				return;

			auto image = ReadImage(body, settings.type, settings.fileNameGetter(completeFileName, name));
			if (image.isNull())
				return;

			if (settings.grayscale)
				image.convertTo(QImage::Format::Format_Grayscale8);

			if (image.pixelFormat().colorModel() != QPixelFormat::Grayscale)
				image = Flibrary::HasAlpha(image, body.constData());

			const auto pixelFormat = image.pixelFormat();
			const bool hasAlpha = pixelFormat.alphaUsage() == QPixelFormat::UsesAlpha;
			if (image.width() > settings.maxSize.width() || image.height() > settings.maxSize.height())
				image = image.scaled(settings.maxSize.width(), settings.maxSize.height(), Qt::KeepAspectRatio, hasAlpha ? Qt::FastTransformation : Qt::SmoothTransformation);

			m_hash.reset();
			for (auto h = 0, height = image.height(), width = image.width(); h < height; ++h)
				m_hash.addData(QByteArrayView { std::bit_cast<const char*>(image.constScanLine(h)), static_cast<qsizetype>(width) * pixelFormat.channelCount() });
			auto hash = QString::fromUtf8(m_hash.result().toHex());
			if (const auto it = uniqueData.find(hash); it != uniqueData.end())
			{
				if (isCover)
					it->second = -1;

				idToNum.try_emplace(std::move(name), it->second);
				return;
			}

			const auto num = uniqueData.try_emplace(std::move(hash), isCover ? -1 : static_cast<int>(uniqueData.size())).first->second;
			auto imageFile = settings.fileNameGetter(completeFileName, isCover ? name : QString::number(num));
			idToNum.try_emplace(std::move(name), num);

			(isCover ? m_covers : m_images).emplace_back(imageFile, encode(settings, imageFile, image, body), dateTime);
		};

		Fb2ImageParser::Parse(input, std::move(binaryCallback));

		input.seek(0);
		Fb2Parser::Parse(inputFilePath, input, output, idToNum);

		return bodyOutput;
	}

	QImage ReadImage(QByteArray& body, const char* imageType, const QString& imageFile) const
	{
		struct Signature
		{
			const char* extension;
			const char* signature;
		};

		static constexpr Signature signatures[] {
			{ "jpg", "\xFF\xD8\xFF\xE0" },
			{ "png", "\x89\x50\x4E\x47" },
		};
		static constexpr Signature unsupportedSignatures[] {
			{ "riff", R"(RIFF)" },
		};
		static constexpr Signature knownSignatures[] {
			{ "html", R"(<html)" },
			{  "xml", R"(<?xml)" },
			{  "svg",  R"(<svg)" },
		};

		auto [image, errorString] = ToImage(body);
		if (!image.isNull())
			return image;

		if (body.size() < m_settings.minImageFileSize)
		{
			PLOGW << QString("%1 %2 too small file size: %3").arg(imageType).arg(imageFile).arg(body.size());
			return {};
		}

		if (const auto it = std::ranges::find_if(signatures, [&](const auto& item) { return body.startsWith(item.signature); }); it != std::end(signatures))
			return AddError(imageType, imageFile, body, QString("%1 %2 may be damaged: %3").arg(imageType).arg(imageFile).arg(errorString), it->extension);

		if (const auto it = std::ranges::find_if(unsupportedSignatures, [&](const auto& item) { return body.startsWith(item.signature); }); it != std::end(unsupportedSignatures))
			return AddError(imageType, imageFile, body, QString("possibly an %1 %2 in %3 format").arg(imageType).arg(imageFile).arg(it->extension), it->extension);

		if (const auto it = std::ranges::find_if(knownSignatures, [&](const auto& item) { return body.startsWith(item.signature); }); it != std::end(knownSignatures))
			return AddError(imageType, imageFile, body, QString("%1 %2 is %3").arg(imageType).arg(imageFile).arg(it->extension), it->extension, false);

		if (QString::fromUtf8(body).contains("!doctype html", Qt::CaseInsensitive))
			return AddError(imageType, imageFile, body, QString("possibly an %1 %2 in %3 format").arg(imageType).arg(imageFile).arg("html"), "html", false);

		return AddError(imageType, imageFile, body, QString("%1 %2 may be damaged: %3").arg(imageType).arg(imageFile).arg(errorString));
	}

	QImage AddError(const char* imageType, const QString& file, const QByteArray& body, const QString& errorText, const QString& ext = {}, const bool tryToFix = true) const
	{
		if (tryToFix)
			if (auto fixed = TryToFix(imageType, file, body); !fixed.isNull())
				return fixed;

		PLOGW << errorText;
		WriteError(m_settings.dstDir, m_fileSystemGuard, file, ext, body);
		m_hasError = true;
		return {};
	}

	QImage TryToFix(const char* imageType, const QString& imageFile, const QByteArray& body) const
	{
		if (m_settings.ffmpeg.isEmpty())
			return {};

		QProcess process;
		QEventLoop eventLoop;
		const auto args = QStringList() << "-i" << "pipe:0" << "-f" << "mjpeg" << "pipe:1";
		const auto ffmpegFileName = QFileInfo(m_settings.ffmpeg).fileName();

		QByteArray fixed;
		QObject::connect(&process, &QProcess::started, [&] { PLOGI << QString("ffmpeg launched for %1 %2\n%3 %4").arg(imageType, imageFile, ffmpegFileName, args.join(" ")); });
		QObject::connect(&process,
		                 &QProcess::finished,
		                 [&](const int code, const QProcess::ExitStatus)
		                 {
							 if (code == 0)
								 PLOGI << QString("%1 %2 is probably fixed").arg(imageType, imageFile);
							 else
								 PLOGW << QString("Cannot fix %1 %2, ffmpeg finished with %3").arg(imageType, imageFile).arg(code);
							 eventLoop.exit(code);
						 });
		QObject::connect(&process, &QProcess::readyReadStandardError, [&] { PLOGW << "\n" << process.readAllStandardError(); });
		QObject::connect(&process, &QProcess::readyReadStandardOutput, [&] { fixed.append(process.readAllStandardOutput()); });

		process.start(m_settings.ffmpeg, args, QIODevice::ReadWrite);
		process.write(body);
		process.closeWriteChannel();

		eventLoop.exec();

		if (fixed.isEmpty())
			return {};

		auto [image, errorString] = ToImage(fixed);
		if (!errorString.isEmpty())
			PLOGW << errorString;

		return image;
	}

private:
	const Settings& m_settings;
	const QString m_folder;

	std::condition_variable& m_queueCondition;
	std::mutex& m_queueGuard;
	DataItems& m_queue;
	std::mutex& m_fileSystemGuard;

	std::atomic_bool& m_hasError;
	std::atomic_int &m_queueSize, &m_fileCount;

	QCryptographicHash m_hash { QCryptographicHash::Md5 };
	std::vector<DataItem> m_covers;
	std::vector<DataItem> m_images;
	ImageStatistics m_imageStatistics;

	const Util::XmlValidator m_validator;

	IClient& m_client;

	std::thread m_thread;
};

QString GetImagesFolder(const QDir& dir, const QString& type)
{
	const QFileInfo fileInfo(dir.path());
	return QString("%1/%2/%3.zip").arg(fileInfo.dir().path(), type, fileInfo.fileName());
}

class FileProcessor final : public Worker::IClient
{
public:
	FileProcessor(const Settings& settings,
	              const QString& folder,
	              std::condition_variable& queueCondition,
	              std::mutex& queueGuard,
	              const int poolSize,
	              std::atomic_int& fileCount,
	              QTextStream* imageStatisticsStream)
		: m_queueCondition { queueCondition }
		, m_queueGuard { queueGuard }
		, m_dstDir { settings.dstDir }
		, m_saveCovers { settings.cover.save }
		, m_saveImages { settings.image.save }
		, m_maxThreadCount { settings.maxThreadCount }
		, m_imageStatisticsStream { imageStatisticsStream }
	{
		for (int i = 0; i < poolSize; ++i)
			m_workers.push_back(std::make_unique<Worker>(settings, folder, m_queueCondition, m_queueGuard, m_queue, m_fileSystemGuard, m_hasError, m_queueSize, fileCount, *this));
	}

public:
	int GetQueueSize() const
	{
		return m_queueSize;
	}

	void Enqueue(QString file, QByteArray data, QDateTime dateTime)
	{
		std::unique_lock lock(m_queueGuard);
		m_queue.emplace(std::move(file), std::move(data), std::move(dateTime));
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
		ArchiveImages(m_saveImages, Global::IMAGES, m_images);
		ArchiveImages(m_saveCovers, Global::COVERS, m_covers);
		WriteImageStatistics();
	}

private:
	void ArchiveImages(const bool saveFlag, const char* type, std::vector<DataItem>& images) const //-V826
	{
		if (!saveFlag || images.empty())
			return;

		PLOGI << "archive " << images.size() << " images: " << GetImagesFolder(m_dstDir, type);
		std::unordered_map<QString, qsizetype> unique;
		for (const auto& image : images)
		{
			auto& item = unique[image.fileName];
			item = std::max(item, image.body.size());
		}

		std::erase_if(images,
		              [&](const auto& image)
		              {
						  const auto it = unique.find(image.fileName);
						  assert(it != unique.end());
						  return image.body.size() < it->second;
					  });

		const auto proj = [](const auto& item) { return item.fileName; };
		std::ranges::sort(images, {}, proj);
		if (const auto range = std::ranges::unique(images, {}, proj); !range.empty())
			images.erase(range.begin(), range.end()); //-V539

		auto zipFiles = Zip::CreateZipFileController();
		for (auto&& image : images)
			zipFiles->AddFile(std::move(image.fileName), std::move(image.body), std::move(image.dateTime));

		Zip zip(GetImagesFolder(m_dstDir, type), Zip::Format::Zip);
		zip.SetProperty(Zip::PropertyId::CompressionLevel, QVariant::fromValue(Zip::CompressionLevel::Ultra));
		zip.SetProperty(Zip::PropertyId::ThreadsCount, m_maxThreadCount);
		zip.Write(std::move(zipFiles));

		images.clear();
	}

	void WriteImageStatistics()
	{
		ScopedCall clearGuard([this] { m_imageStatistics.clear(); });

		if (!m_imageStatisticsStream)
			return;

		for (const auto& [folder, fileName, ordNum, size, width, height, schema, md5] : m_imageStatistics)
			(*m_imageStatisticsStream) << folder << ',' << fileName << ',' << ordNum << ',' << static_cast<int>(schema) << ',' << size << ',' << width << ',' << height << ',' << md5 << '\n';

		m_imageStatisticsStream->flush();
	}

private: // Worker::IClient
	void OnWorkFinished(std::vector<DataItem> covers, std::vector<DataItem> images, ImageStatistics imageStatistics) override
	{
		std::lock_guard lock(m_workClientGuard);
		m_covers.reserve(m_covers.size() + covers.size());
		std::ranges::move(covers, std::back_inserter(m_covers));
		m_images.reserve(m_images.size() + images.size());
		std::ranges::move(images, std::back_inserter(m_images));
		m_imageStatistics.reserve(m_imageStatistics.size() + imageStatistics.size());
		std::ranges::move(imageStatistics, std::back_inserter(m_imageStatistics));
	}

private:
	std::condition_variable& m_queueCondition;
	std::mutex& m_queueGuard;
	std::atomic_bool m_hasError { false };
	DataItems m_queue;
	std::atomic_int m_queueSize { 0 };
	std::mutex m_fileSystemGuard;

	std::mutex m_workClientGuard;

	QDir m_dstDir;
	const bool m_saveCovers;
	const bool m_saveImages;
	const int m_maxThreadCount;

	std::vector<DataItem> m_covers;
	std::vector<DataItem> m_images;
	ImageStatistics m_imageStatistics;
	QTextStream* m_imageStatisticsStream;

	std::vector<std::unique_ptr<Worker>> m_workers;
};

bool ArchiveFb2External(const Settings& settings)
{
	if (!settings.saveFb2 || settings.archiver.isEmpty())
		return false;

	PLOGI << "launching an external archiver";

	QProcess process;
	QEventLoop eventLoop;
	auto args = settings.archiverOptions.split(' ', Qt::SkipEmptyParts);
	for (auto& arg : args)
	{
		arg.replace("%src%", QString("%1/*.fb2").arg(settings.dstDir.path()));
		arg.replace("%dst%", QString("%1").arg(settings.dstDir.path()));
	}

	bool hasErrors = false;

	QObject::connect(&process, &QProcess::started, [&] { PLOGI << "external archiver launched\n" << QFileInfo(settings.archiver).fileName() << " " << args.join(' '); });
	QObject::connect(&process,
	                 &QProcess::finished,
	                 [&](const int code, const QProcess::ExitStatus)
	                 {
						 if (code == 0)
							 PLOGI << QFileInfo(settings.archiver).fileName() << " finished successfully";
						 else
							 PLOGW << QFileInfo(settings.archiver).fileName() << " finished with " << code;
						 eventLoop.exit(code);
					 });
	QObject::connect(&process,
	                 &QProcess::readyReadStandardError,
	                 [&]
	                 {
						 hasErrors = true;
						 PLOGE << process.readAllStandardError();
					 });
	QObject::connect(&process, &QProcess::readyReadStandardOutput, [&] { PLOGI << process.readAllStandardOutput(); });

	process.start(settings.archiver, args, QIODevice::ReadOnly);

	eventLoop.exec();
	return hasErrors;
}

bool ArchiveFb2(const Settings& settings)
{
	if (!settings.archiveFb2)
		return false;

	if (!settings.archiver.isEmpty())
		return ArchiveFb2External(settings);

	auto zipFiles = Zip::CreateZipFileController();
	for (const auto& file : settings.dstDir.entryList({ "*.fb2" }))
		zipFiles->AddFile(settings.dstDir.filePath(file));

	const auto dstArchiveFileName = QString("%1.%2").arg(settings.dstDir.path(), Zip::FormatToString(settings.format));
	PLOGI << "archive " << zipFiles->GetCount() << " fb2: " << dstArchiveFileName;

	Zip zip(dstArchiveFileName, settings.format);
	zip.SetProperty(Zip::PropertyId::CompressionLevel, QVariant::fromValue(Zip::CompressionLevel::Ultra));
	zip.SetProperty(Zip::PropertyId::SolidArchive, false);
	zip.SetProperty(Zip::PropertyId::ThreadsCount, settings.maxThreadCount);
	if (settings.format == Zip::Format::SevenZip)
		zip.SetProperty(Zip::PropertyId::CompressionMethod, QVariant::fromValue(Zip::CompressionMethod::Ppmd));

	const auto result = zip.Write(std::move(zipFiles));
	if (result)
		for (const auto& file : settings.dstDir.entryList({ "*.fb2" }))
			QFile::remove(settings.dstDir.filePath(file));

	return !result;
}

bool ProcessArchiveImpl(const QString& archive, Settings settings, std::atomic_int& fileCount, QTextStream* imageStatisticsStream)
{
	const QFileInfo fileInfo(archive);
	settings.dstDir = QDir(settings.dstDir.filePath(fileInfo.completeBaseName()));
	if (!settings.dstDir.exists() && !settings.dstDir.mkpath("."))
	{
		PLOGE << QString("Cannot create folder %1").arg(settings.dstDir.path());
		return true;
	}

	const Zip zip(archive);
	auto fileList = zip.GetFileNameList();
	const auto fileListCount = fileList.size();
	const int currentFileCount = fileCount;
	PLOGI << QString("%1 processing, total files: %2").arg(fileInfo.fileName()).arg(fileListCount);

	auto hasError = [&]
	{
		const auto maxThreadCount = std::min(std::max(settings.maxThreadCount, 1), static_cast<int>(fileListCount));

		std::condition_variable queueCondition;
		std::mutex queueGuard;
		FileProcessor fileProcessor(settings, fileInfo.completeBaseName(), queueCondition, queueGuard, maxThreadCount, fileCount, imageStatisticsStream);

		while (!fileList.isEmpty())
		{
			if (fileProcessor.GetQueueSize() < maxThreadCount * 2)
			{
				const auto input = zip.Read(fileList.front());
				auto body = input->GetStream().readAll();
				if (!body.isEmpty())
				{
					fileProcessor.Enqueue(std::move(fileList.front()), std::move(body), zip.GetFileTime(fileList.front()));
				}
				else
				{
					PLOGW << fileList.front() << " is empty";
					++fileCount;
				}
				fileList.pop_front();
			}
			else
			{
				std::unique_lock lockStart(queueGuard);
				queueCondition.wait(lockStart, [&]() { return fileProcessor.GetQueueSize() < maxThreadCount * 2; });
			}
		}

		for (int i = 0; i < maxThreadCount; ++i)
			fileProcessor.Enqueue({}, {}, {});

		fileProcessor.Wait();
		return fileProcessor.HasError();
	}();

	hasError = ArchiveFb2(settings) || hasError;

	QDir().rmdir(settings.dstDir.path());

	if (fileCount - currentFileCount != fileListCount)
		PLOGE << QString("something strange: %1 files in archive %2 but processed %3").arg(fileListCount).arg(fileInfo.fileName()).arg(fileCount - currentFileCount);

	const auto resultReport = QString("%1 (%2 of %3 files) processed %4").arg(fileInfo.fileName()).arg(fileCount - currentFileCount).arg(fileListCount).arg(hasError ? "with errors" : "successfully");
	if (hasError)
		PLOGW << resultReport;
	else
		PLOGI << resultReport;

	return hasError;
}

bool ProcessArchive(const QString& file, const Settings& settings, std::atomic_int& fileCount, QTextStream* imageStatisticsStream)
{
	try
	{
		return ProcessArchiveImpl(file, settings, fileCount, imageStatisticsStream);
	}
	catch (const std::exception& ex)
	{
		PLOGE << QString("%1 processing failed: %2").arg(file).arg(ex.what());
	}
	catch (...)
	{
		PLOGE << QString("%1 processing failed").arg(file);
	}

	return true;
}

QStringList ProcessArchives(Settings& settings)
{
	if (!settings.dstDir.exists() && !settings.dstDir.mkpath("."))
		throw std::ios_base::failure(QString("Cannot create folder %1").arg(settings.dstDir.path()).toStdString());
	if (settings.cover.save)
		if (const QDir dir(QString("%1/%2").arg(settings.dstDir.path(), Global::COVERS)); !dir.exists() && !dir.mkpath("."))
			throw std::ios_base::failure(QString("Cannot create folder %1").arg(dir.path()).toStdString());
	if (settings.image.save)
		if (const QDir dir(QString("%1/%2").arg(settings.dstDir.path(), Global::IMAGES)); !dir.exists() && !dir.mkpath("."))
			throw std::ios_base::failure(QString("Cannot create folder %1").arg(dir.path()).toStdString());

	QStringList files;
	for (const auto& wildCard : settings.inputWildcards)
		files << Util::ResolveWildcard(wildCard);

	PLOGD << "Total file count calculation";
	settings.totalFileCount = std::accumulate(files.cbegin(),
	                                          files.cend(),
	                                          settings.totalFileCount,
	                                          [](const auto init, const QString& file)
	                                          {
												  const Zip zip(file);
												  return init + zip.GetFileNameList().size();
											  });
	PLOGI << "Total file count: " << settings.totalFileCount;

	std::unique_ptr<QTextStream> imageStatisticsStream;
	QFile imageStatisticsFile(settings.imageStatistics);
	if (!settings.imageStatistics.isEmpty())
	{
		if (!imageStatisticsFile.open(QIODevice::WriteOnly))
			throw std::ios_base::failure(QString("Cannot write to %1").arg(settings.imageStatistics).toStdString());
		imageStatisticsStream = std::make_unique<QTextStream>(&imageStatisticsFile);
		*imageStatisticsStream << "#archive,fb2_file,ord_num,pixel_type,image_file_size,Width,height,hash";
	}

	std::atomic_int fileCount;
	QStringList failed;
	for (auto&& file : files)
		if (ProcessArchive(file, settings, fileCount, imageStatisticsStream.get()))
			failed << std::move(file);

	return failed;
}

template <typename T>
bool SetValue(const QCommandLineParser& parser, const char* key, T& value) = delete;

template <>
bool SetValue<int>(const QCommandLineParser& parser, const char* key, int& value)
{
	bool ok = false;
	if (const auto parsed = parser.value(key).toInt(&ok); ok)
		value = parsed;

	return ok;
}

template <>
bool SetValue<QSize>(const QCommandLineParser& parser, const char* key, QSize& value)
{
	const auto parsed = parser.value(key).split(',', Qt::SkipEmptyParts);
	if (parsed.isEmpty())
		return false;

	if (parsed.size() < 2)
	{
		bool ok = false;

		if (const auto v = parsed.front().toInt(&ok); !ok)
		{
			return false;
		}
		else
		{
			value.setWidth(v);
			value.setHeight(v);
		}

		return true;
	}

	QSize v;
	bool ok = false;

	if (v.setWidth(parsed.front().toInt(&ok)); !ok)
		return false;

	if (v.setHeight(parsed.back().toInt(&ok)); !ok)
		return false;

	value = v;
	return true;
}

struct CommandLineSettings
{
	Settings settings;
	bool gui { true };
};

CommandLineSettings ProcessCommandLine(const QCoreApplication& app)
{
	Settings settings {};

	QCommandLineParser parser;
	parser.setApplicationDescription(QString("%1 extracts images from *.fb2").arg(APP_ID));
	parser.addHelpOption();
	parser.addVersionOption();
	parser.addOptions({
		{ { "o", FOLDER }, "Output folder (required)", FOLDER },
		{ { QString(QUALITY[0]), QUALITY_OPTION_NAME }, "Compression quality [0, 100] or -1 for default compression quality", QUALITY },
		{ { QString(THREADS[0]), MAX_THREAD_COUNT_OPTION_NAME }, "Maximum number of CPU threads", QString(THREADS).arg(settings.maxThreadCount) },
		{ { QString(FORMAT[0]), FORMAT }, "Output fb2 archive format [7z | zip]", QString("%1 [%2]").arg(FORMAT, "7z") },
		{ { QString(ARCHIVER_OPTION_NAME[0]), ARCHIVER_OPTION_NAME }, "Path to external archiver executable", QString("%1 [embedded zip archiver]").arg(PATH) },

		{ ARCHIVER_COMMANDLINE_OPTION_NAME, "External archiver command line options", COMMANDLINE },
		{ COVER_QUALITY_OPTION_NAME, "Covers compression quality", QUALITY },
		{ IMAGE_QUALITY_OPTION_NAME, "Images compression quality", QUALITY },
		{ MAX_SIZE_OPTION_NAME, "Maximum any images size", SIZE },
		{ MAX_COVER_SIZE_OPTION_NAME, "Maximum cover size", SIZE },
		{ MAX_IMAGE_SIZE_OPTION_NAME, "Maximum image size", SIZE },

		{ MIN_IMAGE_FILE_SIZE_OPTION_NAME, "Minimum image file size threshold for writing to error folder", QString("size [%1]").arg(settings.minImageFileSize) },
		{ FFMPEG_OPTION_NAME, "Path to ffmpeg executable", PATH },
		{ IMAGE_STATISTICS, "Image statistics output path", PATH },

		{ { QString(GRAYSCALE_OPTION_NAME[0]), GRAYSCALE_OPTION_NAME }, "Convert all images to grayscale" },
		{ COVER_GRAYSCALE_OPTION_NAME, "Convert covers to grayscale" },
		{ IMAGE_GRAYSCALE_OPTION_NAME, "Convert images to grayscale" },

		{ NO_ARCHIVE_FB2_OPTION_NAME, "Don't archive fb2" },
		{ NO_FB2_OPTION_NAME, "Don't save fb2" },
		{ NO_IMAGES_OPTION_NAME, "Don't save image" },
		{ COVERS_ONLY_OPTION_NAME, "Save covers only" },
		{ GUI_MODE_OPTION_NAME, "GUI mode" },
	});
	parser.process(app);

	settings.dstDir = parser.value(FOLDER);

	settings.ffmpeg = parser.value(FFMPEG_OPTION_NAME);
	settings.archiver = parser.value(ARCHIVER_OPTION_NAME);
	settings.archiverOptions = parser.value(ARCHIVER_COMMANDLINE_OPTION_NAME);

	if (parser.isSet(FORMAT))
		settings.format = Zip::FormatFromString(parser.value(FORMAT));

	QSize size;
	if (SetValue(parser, MAX_SIZE_OPTION_NAME, size))
		settings.cover.maxSize = settings.image.maxSize = size;
	SetValue(parser, MAX_COVER_SIZE_OPTION_NAME, settings.cover.maxSize);
	SetValue(parser, MAX_IMAGE_SIZE_OPTION_NAME, settings.image.maxSize);

	int quality = -1;
	if (SetValue(parser, QUALITY_OPTION_NAME, quality))
		settings.cover.quality = settings.image.quality = quality;
	SetValue(parser, COVER_QUALITY_OPTION_NAME, settings.cover.quality);
	SetValue(parser, IMAGE_QUALITY_OPTION_NAME, settings.image.quality);

	SetValue(parser, MAX_THREAD_COUNT_OPTION_NAME, settings.maxThreadCount);
	SetValue(parser, MIN_IMAGE_FILE_SIZE_OPTION_NAME, settings.minImageFileSize);

	settings.imageStatistics = parser.value(IMAGE_STATISTICS);

	settings.cover.grayscale = settings.image.grayscale = parser.isSet(GRAYSCALE_OPTION_NAME);
	if (parser.isSet(COVER_GRAYSCALE_OPTION_NAME))
		settings.cover.grayscale = true;
	if (parser.isSet(IMAGE_GRAYSCALE_OPTION_NAME))
		settings.image.grayscale = true;

	settings.saveFb2 = !parser.isSet(NO_FB2_OPTION_NAME);
	settings.archiveFb2 = settings.saveFb2 && !parser.isSet(NO_ARCHIVE_FB2_OPTION_NAME);

	settings.cover.save = settings.image.save = !parser.isSet(NO_IMAGES_OPTION_NAME);
	settings.image.save = settings.image.save && !parser.isSet(COVERS_ONLY_OPTION_NAME);

	std::ranges::transform(parser.positionalArguments(), std::back_inserter(settings.inputWildcards), [](const auto& fileName) { return QDir::fromNativeSeparators(fileName); });

	return CommandLineSettings { std::move(settings), parser.isSet(GUI_MODE_OPTION_NAME) };
}

bool run(int argc, char* argv[])
{
	const QApplication app(argc, argv); //-V821
	QCoreApplication::setApplicationName(APP_ID);
	QCoreApplication::setApplicationVersion(PRODUCT_VERSION);
	Util::XMLPlatformInitializer xmlPlatformInitializer;

	CommandLineSettings settings;

	if (argc > 1)
		settings = ProcessCommandLine(app);

	if (settings.gui)
	{
		PLOGI << "GUI mode activated";

		std::shared_ptr<Hypodermic::Container> container;
		{
			Hypodermic::ContainerBuilder builder;
			DiInit(builder, container);
		}

		const auto translators = Loc::LoadLocales(*container->resolve<ISettings>()); //-V808
		const auto mainWindow = container->resolve<MainWindow>();
		mainWindow->SetSettings(&settings.settings);
		mainWindow->show();
		if (!QApplication::exec())
			return false;
	}

	{
		std::ostringstream stream;
		stream << "Process started with " << settings.settings;
		PLOGI << stream.str();
	}

	const auto checkExternalUtil = [](const QString& name, const QString& path)
	{
		if (!(path.isEmpty() || QFile::exists(path)))
			throw std::invalid_argument(QString("Cannot find %1, path '%2' not found").arg(name).arg(path).toStdString());
	};
	checkExternalUtil("ffmpeg", settings.settings.ffmpeg);
	checkExternalUtil("external archiver", settings.settings.archiver);

	const auto failedArchives = ProcessArchives(settings.settings);
	if (failedArchives.isEmpty())
		return false;

	PLOGE << "Processed with errors:\n" << failedArchives.join("\n");
	return true;
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
