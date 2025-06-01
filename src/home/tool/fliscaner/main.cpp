#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
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
constexpr auto APP_ID = "fliscaner";

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
	size_t m_counter { 0 };
	QEventLoop m_eventLoop;
};

QJsonObject ReadConfig(QFile& file)
{
	assert(file.exists());
	[[maybe_unused]] const auto ok = file.open(QIODevice::ReadOnly);
	assert(ok);

	QJsonParseError jsonParseError;
	const auto doc = QJsonDocument::fromJson(file.readAll(), &jsonParseError);
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
	if (QFile file(path + "/config.json"); file.exists())
		return ReadConfig(file);

	QFile file(":/config/config.json");
	return ReadConfig(file);
}

QString GetDownloadFileName(const QString& fileName)
{
	return QDir(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).filePath(fileName);
}

template <typename T>
void KillMe(T* obj);

struct Task
{
	EventLooper& eventLooper;
	Network::Downloader downloader;
	std::unique_ptr<QIODevice> stream;

	Task(const QString& path, const QString& file, EventLooper& eventLooper, std::unique_ptr<QIODevice> stream, std::function<void()> callback)
		: eventLooper { eventLooper }
		, stream { std::move(stream) }
	{
		eventLooper.Add();

		downloader.Download(
			path + file,
			*this->stream,
			[this_ = this, file, callback = std::move(callback)](size_t, const int code, const QString& message)
			{
				PLOGI << file << " finished " << (code ? "with " + message : "successfully");
				if (code == 0)
				{
					this_->stream.reset();
					callback();
				}

				KillMe(this_);
			},
			[this, file, pct = int64_t { 0 }](const int64_t bytesReceived, const int64_t bytesTotal, bool& /*stopped*/) mutable
			{
				if (const auto currentPct = 100LL * bytesReceived / bytesTotal; currentPct != pct)
				{
					pct = currentPct;
					PLOGI << file << " " << bytesReceived << " (" << bytesTotal << ") " << pct << "%";
				}
			});

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
	QTimer::singleShot(0, [obj] { delete obj; });
}

void ScanSql(const QJsonValue& value, EventLooper& eventLooper)
{
	assert(value.isObject());
	const auto obj = value.toObject();
	const auto path = obj["path"].toString();
	for (const auto fileObj : obj["file"].toArray())
	{
		const auto file = fileObj.toString();
		auto tmpFile = GetDownloadFileName(file + ".tmp"), dstFile = GetDownloadFileName(file);
		if (QFile::exists(dstFile))
		{
			PLOGW << dstFile << " already exists";
			continue;
		}

		auto stream = std::make_unique<QFile>(tmpFile);
		if (!stream->open(QIODevice::WriteOnly))
		{
			PLOGW << "cannot open " << tmpFile;
			continue;
		}

		new Task(path, file, eventLooper, std::move(stream), [tmpFile = std::move(tmpFile), dstFile = std::move(dstFile)] { QFile::rename(tmpFile, dstFile); });
	}
}

void ScanZip(const QJsonArray& regexps, EventLooper& eventLooper, const QString& path, const QString& data)
{
	std::unordered_set<QString> files;
	for (const auto regexpObj : regexps)
	{
		const auto regexp = regexpObj.toString();
		QRegularExpression rx(regexp);
		for (const auto& match : rx.globalMatch(data))
			files.insert(match.captured(0));
	}

	QJsonArray filesArray;
	std::ranges::copy(files, std::back_inserter(filesArray));
	QJsonObject obj {
		{ "path",       path },
		{ "file", filesArray }
	};

	ScanSql(obj, eventLooper);
}

void ScanZip(const QJsonValue& value, EventLooper& eventLooper)
{
	assert(value.isObject());
	const auto obj = value.toObject();
	const auto path = obj["path"].toString();
	const auto file = obj["file"].toString();
	auto regexp = obj["regexp"];
	assert(regexp.isArray());

	auto page = std::make_shared<QByteArray>();
	auto stream = std::make_unique<QBuffer>(page.get());
	stream->open(QIODevice::WriteOnly);

	new Task(path, file, eventLooper, std::move(stream), [&, regexps = regexp.toArray(), path = path + file, page = std::move(page)] { ScanZip(regexps, eventLooper, path, QString::fromUtf8(*page)); });
}

void ScanStub(const QJsonValue&, EventLooper&)
{
	PLOGE << "unexpected parameter";
}

constexpr std::pair<const char*, void (*)(const QJsonValue&, EventLooper&)> SCANNERS[] {
	{ "zip", &ScanZip },
	{ "sql", &ScanSql },
};

} // namespace

int main(int argc, char* argv[])
{
	const QCoreApplication app(argc, argv);

	Log::LoggingInitializer logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, APP_ID).toStdWString());
	plog::ConsoleAppender<Util::LogConsoleFormatter> consoleAppender;
	Log::LogAppender logConsoleAppender(&consoleAppender);
	PLOGI << APP_ID << " started";

	const auto config = ReadConfig(QFileInfo(QString(argv[0])).path());

	EventLooper evenLooper;

	for (size_t i = 1; i < static_cast<size_t>(argc); ++i)
	{
		const auto invoker = FindSecond(SCANNERS, argv[i], &ScanStub, PszComparer {});
		PLOGI << argv[i] << " in process";
		std::invoke(invoker, config[argv[i]], evenLooper);
	}

	return evenLooper.Start();
}
