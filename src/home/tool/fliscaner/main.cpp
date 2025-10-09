#include <unordered_set>

#include <QBuffer>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QString>
#include <QTimer>

#include <plog/Appenders/ConsoleAppender.h>

#include "fnd/FindPair.h"

#include "logging/LogAppender.h"
#include "logging/init.h"
#include "network/network/downloader.h"
#include "util/LogConsoleFormatter.h"

#include "log.h"

#include "config/version.h"

using namespace HomeCompa;

namespace
{
constexpr auto APP_ID        = "fliscaner";
constexpr auto OUTPUT_FOLDER = "output-folder";
constexpr auto CONFIG        = "config";
QString        DST_PATH;

class EventLooper
{
public:
	void Add()
	{
		++m_counter;
	}

	void Release()
	{
		if (--m_counter == 0)
			m_eventLoop.exit();
	}

	int Start()
	{
		return m_counter ? m_eventLoop.exec() : 0;
	}

private:
	size_t     m_counter { 0 };
	QEventLoop m_eventLoop;
};

QJsonObject ReadConfig(QFile& file)
{
	assert(file.exists());
	[[maybe_unused]] const auto ok = file.open(QIODevice::ReadOnly);
	assert(ok);

	QJsonParseError jsonParseError;
	const auto      doc = QJsonDocument::fromJson(file.readAll(), &jsonParseError);
	if (jsonParseError.error != QJsonParseError::NoError)
	{
		PLOGE << jsonParseError.errorString();
		return {};
	}

	assert(doc.isObject());
	return doc.object();
}

QJsonObject ReadConfig(const QString& path)
{
	if (QFile file(path); file.exists())
		return ReadConfig(file);

	QFile file(":/config/config.json");
	return ReadConfig(file);
}

QString GetDownloadFileName(const QString& fileName)
{
	return QDir(DST_PATH).filePath(fileName);
}

template <typename T>
void KillMe(T* obj);

struct Task
{
	EventLooper&               eventLooper;
	Network::Downloader        downloader;
	std::unique_ptr<QIODevice> stream;

	Task(const QString& path, const QString& file, EventLooper& eventLooper, std::unique_ptr<QIODevice> stream, std::function<void()> callback)
		: eventLooper { eventLooper }
		, stream { std::move(stream) }
	{
		eventLooper.Add();

		downloader.Download(
			path + file,
			*this->stream,
			[this_ = this, file, callback = std::move(callback)](size_t, const int code, const QString& message) {
				PLOGI << file << " finished " << (code ? "with " + message : "successfully");
				if (code == 0)
				{
					this_->stream.reset();
					callback();
				}

				KillMe(this_);
			},
			[this, file, pct = int64_t { 0 }](const int64_t bytesReceived, const int64_t bytesTotal, bool& /*stopped*/) mutable {
				if (const auto currentPct = 100LL * bytesReceived / bytesTotal; currentPct != pct)
				{
					pct = currentPct;
					PLOGI << file << " " << bytesReceived << " (" << bytesTotal << ") " << pct << "%";
				}
			}
		);

		PLOGI << file << " started";
	}

	~Task()
	{
		eventLooper.Release();
	}

	NON_COPY_MOVABLE(Task)
};

template <typename T>
void KillMe(T* obj)
{
	QTimer::singleShot(0, [obj] {
		delete obj;
	});
}

void GetFiles(const QJsonValue& value, EventLooper& eventLooper)
{
	assert(value.isObject());
	const auto obj  = value.toObject();
	const auto path = obj["path"].toString();
	for (const auto fileObj : obj["file"].toArray())
	{
		const auto file    = fileObj.toString();
		auto       tmpFile = GetDownloadFileName(file + ".tmp"), dstFile = GetDownloadFileName(file);
		if (QFile::exists(dstFile))
		{
			PLOGW << file << " already exists";
			continue;
		}

		auto stream = std::make_unique<QFile>(tmpFile);
		if (!stream->open(QIODevice::WriteOnly))
		{
			PLOGW << "cannot open " << tmpFile;
			continue;
		}

		new Task(path, file, eventLooper, std::move(stream), [tmpFile = std::move(tmpFile), dstFile = std::move(dstFile)] {
			QFile::rename(tmpFile, dstFile);
		});
	}
}

void GetDaily(const QJsonArray& regexps, EventLooper& eventLooper, const QString& path, const QString& data)
{
	std::unordered_set<QString> files;
	for (const auto regexpObj : regexps)
	{
		const auto         regexp = regexpObj.toString();
		QRegularExpression rx(regexp);
		for (const auto& match : rx.globalMatch(data))
			files.insert(match.captured(0));
	}

	QJsonArray filesArray;
	std::ranges::copy(files, std::back_inserter(filesArray));
	const QJsonObject obj {
		{ "path",       path },
		{ "file", filesArray }
	};

	GetFiles(obj, eventLooper);
}

void GetDaily(const QJsonValue& value, EventLooper& eventLooper)
{
	assert(value.isObject());
	const auto obj    = value.toObject();
	const auto path   = obj["path"].toString();
	const auto file   = obj["file"].toString();
	const auto regexp = obj["regexp"];
	assert(regexp.isArray());

	auto page   = std::make_shared<QByteArray>();
	auto stream = std::make_unique<QBuffer>(page.get());
	stream->open(QIODevice::WriteOnly);

	new Task(path, file, eventLooper, std::move(stream), [&, regexps = regexp.toArray(), path = path + file, page = std::move(page)] {
		GetDaily(regexps, eventLooper, path, QString::fromUtf8(*page));
	});
}

void ScanStub(const QJsonValue&, EventLooper&)
{
	PLOGE << "unexpected parameter";
}

constexpr std::pair<const char*, void (*)(const QJsonValue&, EventLooper&)> SCANNERS[] {
	{ "zip", &GetDaily },
	{ "sql", &GetFiles },
};

} // namespace

int main(int argc, char* argv[])
{
	const QCoreApplication app(argc, argv);
	QCoreApplication::setApplicationName(APP_ID);
	QCoreApplication::setApplicationVersion(PRODUCT_VERSION);

	DST_PATH = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);

	Log::LoggingInitializer                          logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, APP_ID).toStdWString());
	plog::ConsoleAppender<Util::LogConsoleFormatter> consoleAppender;
	Log::LogAppender                                 logConsoleAppender(&consoleAppender);
	PLOGI << APP_ID << " started";

	QCommandLineParser parser;
	parser.setApplicationDescription(QString("%1 downloads files from Flibusta").arg(APP_ID));
	parser.addHelpOption();
	parser.addVersionOption();
	parser.addOptions({
		{ { "o", OUTPUT_FOLDER }, "Output folder",                                                  DST_PATH },
		{        { "c", CONFIG },        "Config", "Apply config or extract it from resources if not exists" },
	});
	parser.addPositionalArgument("sql", "Download dump files");
	parser.addPositionalArgument("zip", "Download book archives");
	parser.process(app);

	if (parser.isSet(OUTPUT_FOLDER))
		DST_PATH = parser.value(OUTPUT_FOLDER);
	if (const QDir dir(DST_PATH); !dir.exists())
		if (!dir.mkpath("."))
		{
			PLOGE << "Cannot create " << DST_PATH;
			return 1;
		}

	auto configFileName = QFileInfo(QString(argv[0])).dir().filePath("config.json");
	if (parser.isSet(CONFIG))
	{
		if (auto value = parser.value(CONFIG); QFile::exists(value))
		{
			configFileName = std::move(value);
		}
		else
		{
			QFile inp(":/config/config.json");
			(void)inp.open(QIODevice::ReadOnly);
			QFile outp(GetDownloadFileName("config.json"));
			(void)outp.open(QIODevice::WriteOnly);
			outp.write(inp.readAll());
		}
	}

	const auto config = ReadConfig(configFileName);

	EventLooper evenLooper;

	for (const auto& arg : parser.positionalArguments())
	{
		const auto invoker = FindSecond(SCANNERS, arg.toStdString().data(), &ScanStub, PszComparer {});
		PLOGI << arg << " in process";
		std::invoke(invoker, config[arg], evenLooper);
	}

	const auto result = evenLooper.Start();
	PLOGI << APP_ID << " finished with " << result;
	return result;
}
