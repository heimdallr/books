#include <filesystem>

#include <QCoreApplication>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>

#include "quazip.h"

#include <plog/Log.h>

#include "constants/ProductConstant.h"
#include "util/Settings.h"

#include "FileDialogProvider.h"

#include "ReaderController.h"

namespace HomeCompa::Flibrary {

namespace {

auto CreateTempFileName(const std::string & file)
{
	const QFileInfo fileInfo(QString::fromStdString(file));
	QTemporaryFile temporaryFile(QString("%1/%2.%3.XXXXXX.%4").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), Constant::COMPANY_ID, Constant::PRODUCT_ID, fileInfo.suffix()));
	temporaryFile.setAutoRemove(false);
	if (!temporaryFile.open())
		throw std::runtime_error("Cannot create temporary file " + temporaryFile.fileName().toStdString());

	return temporaryFile.fileName();
}

void Extract(const QString & dstFileName, const std::filesystem::path & archive, const std::string & file)
{
	QuaZip zip(QString::fromStdWString(archive));
	if (!zip.open(QuaZip::mdUnzip))
		throw std::runtime_error("Cannot open " + archive.string());

	if (!zip.setCurrentFile(QString::fromStdString(file)))
		throw std::runtime_error("Cannot find " + file + " in " + archive.string());

	QuaZipFile zipFile(&zip);
	if (!zipFile.open(QIODevice::ReadOnly))
		throw std::runtime_error("Cannot open " + file + " in " + archive.string());

	QFile temporaryFile(dstFileName);
	if (!temporaryFile.open(QIODevice::WriteOnly))
		throw std::runtime_error("Cannot create temporary file " + dstFileName.toStdString());

	const auto buffer = zipFile.readAll();
	if (buffer.size() <= 0)
		throw std::runtime_error("Cannot read " + file + " in " + archive.string());

	if (temporaryFile.write(buffer) != buffer.size())
		throw std::runtime_error("Cannot write temporary file" + temporaryFile.fileName().toStdString());

	PLOGV << "Book temporary saved as " << temporaryFile.fileName().toStdString();
}

struct TemporaryFile
{
	QString name;

	explicit TemporaryFile(const std::string & file)
		: name(CreateTempFileName(file))
	{
	}

	~TemporaryFile()
	{
		if (!name.isEmpty())
			QFile::remove(name);
	}

	NON_COPY_MOVABLE(TemporaryFile)
};

class ReaderSession
{
	NON_COPY_MOVABLE(ReaderSession)

public:
	class IObserver
	{
	public:
		virtual ~IObserver() = default;
		virtual void HandleReaderSessionFinished(ReaderSession * readerSession) = 0;
	};

	ReaderSession(IObserver & observer, const QString & readerPath, const std::filesystem::path & archive, const std::string & file)
		: m_observer(observer)
		, m_tempFile(file)
	{
		Extract(m_tempFile.name, archive, file);

		QObject::connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), [&] (int exitCode, QProcess::ExitStatus exitStatus)
		{
			PLOGD << "Reader finished with " << exitCode << ": " << (exitStatus == QProcess::NormalExit ? "normal" : "crash");
			m_observer.HandleReaderSessionFinished(this);
		});

		QObject::connect(&m_process, &QProcess::started, [] { PLOGD << "Reader started"; });

		m_process.start(readerPath, { m_tempFile.name });

		PLOGV << "Reader session created";
	}

	~ReaderSession()
	{
		m_process.close();
		m_process.waitForFinished();

		PLOGV << "Reader session destroyed";
	}

private:
	QProcess m_process;
	IObserver & m_observer;
	TemporaryFile m_tempFile;
};

}

class ReaderController::Impl
	: virtual public ReaderSession::IObserver
{
public:
	explicit Impl()
	{
		m_settings.BeginGroup("Reader");
	}

	void StartReader(const std::filesystem::path & archive, const std::string & file)
	{
		PLOGV << "Start reader for " << archive.string() << "#" << file;
		const auto ext = QFileInfo(QString::fromStdString(file)).suffix();
		const auto readerPath = GetReaderPath(ext);
		if (readerPath.isEmpty())
		{
			PLOGD << "Reader not found";
			return;
		}

		PLOGV << "Reader found: " << readerPath;

		assert(QFile::exists(readerPath));
		try
		{
			auto readerSession = std::make_unique<ReaderSession>(*this, readerPath, archive, file);
			m_readSessions.emplace(readerSession.get(), std::move(readerSession));
		}
		catch(const std::exception & ex)
		{
			PLOGE << ex.what();
			m_settings.Remove(ext);
		}
	}

private: // ReaderSession::Observer
	void HandleReaderSessionFinished(ReaderSession * readerSession) override
	{
		QTimer::singleShot(0, [readerSession, this]
		{
			m_readSessions.erase(readerSession);
		});
	}

private:
	QString GetReaderPath(const QString & ext)
	{
		auto path = m_settings.Get(ext).toString();
		if (!path.isEmpty() && QFile::exists(path))
			return path;

		path = FileDialogProvider::SelectFile(QCoreApplication::translate("FileDialog", "Select %1 reader").arg(ext), path, QCoreApplication::translate("FileDialog", "Applications (*.exe)"));
		if (!QFile::exists(path))
			path.clear();

		if (!path.isEmpty())
			m_settings.Set(ext, path);

		return path;
	}

private:
	Settings m_settings { Constant::COMPANY_ID, Constant::PRODUCT_ID };
	std::unordered_map<ReaderSession *, std::unique_ptr<ReaderSession>> m_readSessions;
};

ReaderController::ReaderController() = default;
ReaderController::~ReaderController() = default;

void ReaderController::StartReader(const std::filesystem::path & archive, const std::string & file)
{
	m_impl->StartReader(archive, file);
}

}
