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

constexpr auto READER_KEY = "Reader/%1";
constexpr auto DIALOG_KEY = "Reader";
constexpr auto DEFAULT = "default";

TR_DEF

class ReaderProcess final : QProcess
{
public:
	ReaderProcess(const QString& process, const QString& fileName, std::shared_ptr<QTemporaryDir> temporaryDir, QObject* parent = nullptr)
		: QProcess(parent)
		, m_temporaryDir(std::move(temporaryDir))
	{
		start(process, { fileName });
		waitForStarted();
		connect(this, &QProcess::finished, this, &QObject::deleteLater);
	}

private:
	std::shared_ptr<QTemporaryDir> m_temporaryDir;
};

std::shared_ptr<QTemporaryDir> Extract(const ILogicFactory& logicFactory, const QString& archive, QString& fileName, QString& error)
{
	try
	{
		const Zip zip(archive);
		const auto stream = zip.Read(fileName);
		auto temporaryDir = logicFactory.CreateTemporaryDir();
		const auto fileNameDst = temporaryDir->filePath(fileName);
		if (QFile file(fileNameDst); file.open(QIODevice::WriteOnly))
			file.write(RestoreImages(stream->GetStream(), archive, fileName));

		fileName = fileNameDst;
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
	auto ext = QFileInfo(fileName).suffix();
	auto key = QString(READER_KEY).arg(ext);
	auto reader = m_impl->settings->Get(key).toString();
	if (reader.isEmpty())
	{
		switch (m_impl->uiFactory->ShowQuestion(Tr(USE_DEFAULT), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes)) // NOLINT(clang-diagnostic-switch-enum)
		{
			case QMessageBox::Yes:
				reader = DEFAULT;
				break;
			case QMessageBox::No:
				reader = m_impl->uiFactory->GetOpenFileName(DIALOG_KEY, Tr(DIALOG_TITLE).arg(ext), Tr(DIALOG_FILTER));
				break;
			case QMessageBox::Cancel:
			default:
				return;
		}

		if (reader.isEmpty())
			return;

		m_impl->settings->Set(key, reader);
	}
	assert(!reader.isEmpty());

	auto archive = QString("%1/%2").arg(m_impl->collectionController->GetActiveCollection().folder, folderName);
	std::shared_ptr executor = ILogicFactory::Lock(m_impl->logicFactory)->GetExecutor();
	(*executor)(
		{ "Extract book",
	      [this, executor, ext = std::move(ext), key = std::move(key), reader = std::move(reader), archive = std::move(archive), fileName = std::move(fileName), callback = std::move(callback)]() mutable
	      {
			  QString error;
			  auto temporaryDir = Extract(*ILogicFactory::Lock(m_impl->logicFactory), archive, fileName, error);
			  return [this,
		              executor = std::move(executor),
		              ext = std::move(ext),
		              key = std::move(key),
		              reader = std::move(reader),
		              fileName = std::move(fileName),
		              callback = std::move(callback),
		              temporaryDir = std::move(temporaryDir),
		              error(std::move(error))](size_t) mutable
			  {
				  const ScopedCall callbackGuard([&, callback = std::move(callback)]() mutable { callback(); });

				  if (!temporaryDir)
					  return m_impl->uiFactory->ShowError(error);

				  const auto getReader = [&]
				  {
					  reader = m_impl->uiFactory->GetOpenFileName(DIALOG_KEY, Tr(DIALOG_TITLE).arg(ext), Tr(DIALOG_FILTER));
					  if (!reader.isEmpty())
						  m_impl->settings->Set(key, reader);
				  };

				  if (reader == DEFAULT)
				  {
					  if (QDesktopServices::openUrl(fileName))
						  return;

					  if (m_impl->uiFactory->ShowQuestion(Tr(CANNOT_START_DEFAULT_READER), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes) != QMessageBox::Yes)
						  return;

					  getReader();
					  if (reader.isEmpty())
						  return;
				  }

				  while (!QFile::exists(reader))
				  {
					  if (m_impl->uiFactory->ShowQuestion(Tr(CANNOT_START_READER).arg(QFileInfo(reader).fileName()), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes)
				          != QMessageBox::Yes)
						  return;

					  getReader();
					  if (reader.isEmpty())
						  return;
				  }

				  new ReaderProcess(reader, fileName, std::move(temporaryDir), m_impl->uiFactory->GetParentObject());
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
