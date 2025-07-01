#include "ReaderController.h"

#include <random>

#include <QDesktopServices>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include "fnd/ScopedCall.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/ExportStat.h"
#include "interface/constants/Localization.h"

#include "util/IExecutor.h"
#include "util/PlatformUtil.h"
#include "util/files.h"

#include "ImageRestore.h"
#include "log.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto CONTEXT = "ReaderController";
constexpr auto DIALOG_TITLE = QT_TRANSLATE_NOOP("ReaderController", "Select %1 reader");
constexpr auto DIALOG_FILTER = QT_TRANSLATE_NOOP("ReaderController", "Applications (*.exe)");
constexpr auto USE_DEFAULT = QT_TRANSLATE_NOOP("ReaderController", "Use the default reader?");
constexpr auto CANNOT_START_DEFAULT_READER = QT_TRANSLATE_NOOP("ReaderController", "Cannot start default reader. Will you specify the application manually?");
constexpr auto CANNOT_START_READER = QT_TRANSLATE_NOOP("ReaderController", "'%1' not found. Will you specify another application?");
constexpr auto UNSUPPORTED = QT_TRANSLATE_NOOP("ReaderController", "Unsupported file type");
constexpr auto SELECT_FILE = QT_TRANSLATE_NOOP("ReaderController", "Specify the file to be read");

constexpr auto READER_KEY = "Reader/%1";
constexpr auto DIALOG_KEY = "Reader";
constexpr auto DEFAULT = "default";

TR_DEF

class ReaderProcess final : QProcess
{
public:
	ReaderProcess(const QString& process, const QString& fileName, std::shared_ptr<ILogicFactory::ITemporaryDir> temporaryDir, QObject* parent = nullptr)
		: QProcess(parent)
		, m_temporaryDir(std::move(temporaryDir))
	{
		start(process, { fileName });
		waitForStarted();
		connect(this, &QProcess::finished, this, &QObject::deleteLater);
	}

private:
	std::shared_ptr<ILogicFactory::ITemporaryDir> m_temporaryDir;
};

std::shared_ptr<ILogicFactory::ITemporaryDir> Extract(const ISettings& settings, const ILogicFactory& logicFactory, const QString& archive, QString& fileName, QString& error)
{
	try
	{
		const Zip zip(archive);
		const auto stream = zip.Read(fileName);
		auto temporaryDir = logicFactory.CreateTemporaryDir(true);

		if (Zip::IsArchive(Util::RemoveIllegalPathCharacters(fileName)))
		{
			const Zip subZip(stream->GetStream());
			const auto fileList = subZip.GetFileNameList();
			QStringList filesWithReader;
			for (const auto& archiveFileName : fileList)
			{
				if (auto ext = QFileInfo(archiveFileName).suffix(); !ext.isEmpty())
				{
					auto key = QString(READER_KEY).arg(ext);
					if (auto reader = settings.Get(key).toString(); !reader.isEmpty())
						filesWithReader << archiveFileName;
				}

				auto fileNameDst = temporaryDir->filePath(archiveFileName);
				const auto dir = QFileInfo(fileNameDst).dir();
				if (!dir.exists() && !dir.mkpath("."))
				{
					PLOGE << "Cannot create " << dir.dirName();
					continue;
				}

				if (QFile file(fileNameDst); file.open(QIODevice::WriteOnly))
				{
					const auto subStream = subZip.Read(archiveFileName);
					file.write(RestoreImages(subStream->GetStream(), archive, fileName));
				}
			}

			if (fileList.size() == 1)
				fileName = temporaryDir->filePath(fileList.front());
			else if (filesWithReader.size() == 1)
				fileName = temporaryDir->filePath(filesWithReader.front());
			else
				fileName.clear();
		}
		else
		{
			auto fileNameDst = temporaryDir->filePath(Util::RemoveIllegalPathCharacters(fileName));
			if (QFile file(fileNameDst); file.open(QIODevice::WriteOnly))
				file.write(RestoreImages(stream->GetStream(), archive, fileName));

			fileName = std::move(fileNameDst);
		}
		return temporaryDir;
	}
	catch (const std::exception& ex)
	{
		error = ex.what();
	}

	return {};
}

} // namespace

struct ReaderController::Impl
{
	std::weak_ptr<const ILogicFactory> logicFactory;
	std::shared_ptr<ISettings> settings;
	PropagateConstPtr<ICollectionController, std::shared_ptr> collectionController;
	PropagateConstPtr<IUiFactory, std::shared_ptr> uiFactory;
	PropagateConstPtr<IDatabaseUser, std::shared_ptr> databaseUser;

	mutable std::random_device rd {};
	mutable std::mt19937 mt { rd() };

	Impl(const std::shared_ptr<const ILogicFactory>& logicFactory,
	     std::shared_ptr<ISettings> settings,
	     std::shared_ptr<ICollectionController> collectionController,
	     std::shared_ptr<IUiFactory> uiFactory,
	     std::shared_ptr<IDatabaseUser> databaseUser)
		: logicFactory { logicFactory }
		, settings { std::move(settings) }
		, collectionController { std::move(collectionController) }
		, uiFactory { std::move(uiFactory) }
		, databaseUser { std::move(databaseUser) }
	{
	}

	void Read(std::shared_ptr<ILogicFactory::ITemporaryDir> temporaryDir, QString fileName, Callback callback, const QString& error) const
	{
		const ScopedCall callbackGuard([&, callback = std::move(callback)]() mutable { callback(); });

		if (fileName.isEmpty())
		{
			fileName = uiFactory->GetOpenFileName({}, Tr(SELECT_FILE), {}, temporaryDir->path());
			if (fileName.isEmpty())
				return;
		}

		if (!temporaryDir)
			return uiFactory->ShowError(error);

		auto ext = QFileInfo(fileName).suffix();
		if (ext.isEmpty())
			return uiFactory->ShowError(Tr(UNSUPPORTED));

		auto key = QString(READER_KEY).arg(ext);
		auto reader = settings->Get(key).toString();

		const auto getReader = [&]
		{
			reader = uiFactory->GetOpenFileName(DIALOG_KEY, Tr(DIALOG_TITLE).arg(ext), Tr(DIALOG_FILTER));
			if (!reader.isEmpty())
				settings->Set(key, reader);
		};

		if (reader.isEmpty())
		{
			if (Util::IsRegisteredExtension(ext))
				switch (uiFactory->ShowQuestion(Tr(USE_DEFAULT), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes)) // NOLINT(clang-diagnostic-switch-enum)
				{
					case QMessageBox::Yes:
						settings->Set(key, (reader = DEFAULT));
						break;
					case QMessageBox::No:
						break;
					case QMessageBox::Cancel:
					default:
						return;
				}

			if (reader.isEmpty())
			{
				getReader();
				if (reader.isEmpty())
					return;
			}
		}

		if (reader == DEFAULT)
		{
			if (QDesktopServices::openUrl(fileName))
				return;

			settings->Remove(key);
			if (uiFactory->ShowQuestion(Tr(CANNOT_START_DEFAULT_READER), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes) != QMessageBox::Yes)
				return;

			getReader();
			if (reader.isEmpty())
				return;
		}

		while (!QFile::exists(reader))
		{
			if (uiFactory->ShowQuestion(Tr(CANNOT_START_READER).arg(QFileInfo(reader).fileName()), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes) != QMessageBox::Yes)
				return;

			getReader();
			if (reader.isEmpty())
				return;
		}

		assert(!reader.isEmpty());
		new ReaderProcess(reader, fileName, std::move(temporaryDir), uiFactory->GetParentObject());
	}
};

ReaderController::ReaderController(const std::shared_ptr<const ILogicFactory>& logicFactory,
                                   std::shared_ptr<ISettings> settings,
                                   std::shared_ptr<ICollectionController> collectionController,
                                   std::shared_ptr<IUiFactory> uiFactory,
                                   std::shared_ptr<IDatabaseUser> databaseUser)
	: m_impl(logicFactory, std::move(settings), std::move(collectionController), std::move(uiFactory), std::move(databaseUser))
{
	PLOGV << "ReaderController created";
}

ReaderController::~ReaderController()
{
	PLOGV << "ReaderController destroyed";
}

void ReaderController::Read(const QString& folderName, QString fileName, Callback callback) const
{
	auto archive = QString("%1/%2").arg(m_impl->collectionController->GetActiveCollection().folder, folderName);
	std::shared_ptr executor = ILogicFactory::Lock(m_impl->logicFactory)->GetExecutor();
	(*executor)({ "Extract book",
	              [this, executor, archive = std::move(archive), fileName = std::move(fileName), callback = std::move(callback)]() mutable
	              {
					  QString error;
					  auto temporaryDir = Extract(*m_impl->settings, *ILogicFactory::Lock(m_impl->logicFactory), archive, fileName, error);
					  return [this, executor = std::move(executor), fileName = std::move(fileName), callback = std::move(callback), temporaryDir = std::move(temporaryDir), error(std::move(error))](
								 size_t) mutable
					  {
						  m_impl->Read(std::move(temporaryDir), std::move(fileName), std::move(callback), error);
						  executor.reset();
					  };
				  } });
}

void ReaderController::Read(long long id) const
{
	m_impl->databaseUser->Execute({ "Get archive and file names",
	                                [this, id]()
	                                {
										const auto db = m_impl->databaseUser->Database();
										{
											const auto transaction = db->CreateTransaction();
											const auto command = transaction->CreateQuery(ExportStat::INSERT_QUERY);
											command->Bind(0, id);
											command->Bind(1, static_cast<int>(ExportStat::Type::Read));
											command->Execute();
											transaction->Commit();
										}

										const auto query = db->CreateQuery("select f.FolderTitle, b.FileName||b.Ext from Books b join Folders f on f.FolderID = b.FolderID where b.BookID = ?");
										query->Bind(0, id);
										query->Execute();
										assert(!query->Eof());
										QString folderName = query->Get<const char*>(0), fileName = query->Get<const char*>(1);
										return [this, folderName = std::move(folderName), fileName = std::move(fileName)](size_t) mutable { Read(folderName, std::move(fileName), []() {}); };
									} });
}

void ReaderController::ReadRandomBook(QString lang) const
{
	m_impl->databaseUser->Execute({ "Get books for language",
	                                [this, lang = std::move(lang)]()
	                                {
										std::function<void(size_t)> result { [](size_t) {} };
										const auto db = m_impl->databaseUser->Database();
										const auto query = db->CreateQuery("select f.FolderTitle, b.FileName||b.Ext from Books b join Folders f on f.FolderID = b.FolderID where b.Lang = ?");
										query->Bind(0, lang.toStdString());
										std::vector<std::pair<QString, QString>> allBooks;
										for (query->Execute(); !query->Eof(); query->Next())
											allBooks.emplace_back(query->Get<const char*>(0), query->Get<const char*>(1));

										if (allBooks.empty())
											return result;

										std::uniform_int_distribution<size_t> distribution(0, allBooks.size() - 1);
										const auto n = distribution(m_impl->mt);

										result = [this, folderName = std::move(allBooks[n].first), fileName = std::move(allBooks[n].second)](size_t) mutable
										{ Read(folderName, std::move(fileName), []() {}); };

										return result;
									} });
}
