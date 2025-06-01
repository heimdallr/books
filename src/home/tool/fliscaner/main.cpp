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

struct Task;
void Suicide(Task* task);

struct Task
{
	EventLooper& eventLooper;
	Network::Downloader downloader;
	std::unique_ptr<QIODevice> stream;

	Task(const QString& path, const QString& file, EventLooper& eventLooper, std::unique_ptr<QIODevice> stream)
		: eventLooper { eventLooper }
		, stream { std::move(stream) }
	{
		eventLooper.Add();

		downloader.Download(
			path + file,
			*this->stream,
			[this_ = this, file](size_t, const int code, const QString& message)
			{
				PLOGI << file << " finished " << (code ? "with " + message : "successfully");
				if (code == 0)
				{
					this_->stream.reset();
					const auto tmpFile = GetDownloadFileName(file + ".tmp"), dstFile = GetDownloadFileName(file);
					if (QFile::exists(dstFile))
						QFile::remove(dstFile);
					QFile::rename(tmpFile, dstFile);
				}

				Suicide(this_);
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

void Suicide(Task* task)
{
	QTimer::singleShot(0, [task] { delete task; });
}

void ScanZip(const QJsonValue&, EventLooper&)
{
}

void ScanSql(const QJsonValue& value, EventLooper& eventLooper)
{
	assert(value.isObject());
	const auto obj = value.toObject();
	const auto path = obj["path"].toString();
	for (const auto fileObj : obj["file"].toArray())
	{
		const auto file = fileObj.toString();
		const auto dstFileName = GetDownloadFileName(file + ".tmp");
		auto stream = std::make_unique<QFile>(dstFileName);
		if (!stream->open(QIODevice::WriteOnly))
		{
			PLOGW << "cannot open " << dstFileName;
			continue;
		}

		new Task(path, file, eventLooper, std::move(stream));
	}
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
