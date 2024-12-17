#include "ReaderController.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include <plog/Log.h>

#include "fnd/ScopedCall.h"

#include "interface/constants/Localization.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/ui/IUiFactory.h"

#include "util/IExecutor.h"
#include "util/ISettings.h"

#include "ImageRestore.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto CONTEXT = "ReaderController";
constexpr auto DIALOG_TITLE = QT_TRANSLATE_NOOP("ReaderController", "Select %1 reader");
constexpr auto DIALOG_FILTER = QT_TRANSLATE_NOOP("ReaderController", "Applications (*.exe)");
constexpr auto USE_DEFAULT = QT_TRANSLATE_NOOP("ReaderController", "Use the default reader?");

constexpr auto READER_KEY = "Reader/%1";
constexpr auto DIALOG_KEY = "Reader";
constexpr auto DEFAULT = "default";

TR_DEF

class ReaderProcess final : QProcess
{
public:
	ReaderProcess(const QString & process, const QString & fileName, std::shared_ptr<QTemporaryDir> temporaryDir, QObject * parent = nullptr)
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

std::shared_ptr<QTemporaryDir> Extract(const ILogicFactory& logicFactory, const QString & archive, QString & fileName, QString & error)
{
	try
	{
		const Zip zip(archive);
		auto & stream = zip.Read(fileName);
		auto temporaryDir = logicFactory.CreateTemporaryDir();
		const auto fileNameDst = temporaryDir->filePath(fileName);
		if (QFile file(fileNameDst); file.open(QIODevice::WriteOnly))
			file.write(RestoreImages(stream, archive, fileName));

		fileName = fileNameDst;
		return temporaryDir;
	}
	catch(const std::exception & ex)
	{
		error = ex.what();
	}

	return {};
}

}

struct ReaderController::Impl
{
	std::shared_ptr<ISettings> settings;
	PropagateConstPtr<ICollectionController, std::shared_ptr> collectionController;
	std::weak_ptr<const ILogicFactory> logicFactory;
	PropagateConstPtr<IUiFactory, std::shared_ptr> uiFactory;

	Impl(std::shared_ptr<ISettings> settings
		, std::shared_ptr<ICollectionController> collectionController
		, const std::shared_ptr<const ILogicFactory>& logicFactory
		, std::shared_ptr<IUiFactory> uiFactory
	)
		: settings(std::move(settings))
		, collectionController(std::move(collectionController))
		, logicFactory(logicFactory)
		, uiFactory(std::move(uiFactory))
	{
	}
};

ReaderController::ReaderController(std::shared_ptr<ISettings> settings
	, std::shared_ptr<ICollectionController> collectionController
	, const std::shared_ptr<const ILogicFactory>& logicFactory
	, std::shared_ptr<IUiFactory> uiFactory
)
	: m_impl(std::move(settings)
		, std::move(collectionController)
		, logicFactory
		, std::move(uiFactory)
	)
{
	PLOGV << "ReaderController created";
}

ReaderController::~ReaderController()
{
	PLOGV << "ReaderController destroyed";
}

void ReaderController::Read(const QString & folderName, QString fileName, Callback callback) const
{
	const auto ext = QFileInfo(fileName).suffix();
	const auto key = QString(READER_KEY).arg(ext);
	auto reader = m_impl->settings->Get(key).toString();
	if (reader.isEmpty())
	{
		switch(m_impl->uiFactory->ShowQuestion(Tr(USE_DEFAULT), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes))
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
	}
	if (!reader.isEmpty())
		m_impl->settings->Set(key, reader);

	if (reader.isEmpty())
		return;

	auto archive = QString("%1/%2").arg(m_impl->collectionController->GetActiveCollection().folder, folderName);
	std::shared_ptr executor = ILogicFactory::Lock(m_impl->logicFactory)->GetExecutor();
	(*executor)({ "Extract book", [this
		, executor
		, reader = std::move(reader)
		, archive = std::move(archive)
		, fileName = std::move(fileName)
		, callback = std::move(callback)
	] () mutable
	{
		QString error;
		auto temporaryDir = Extract(*ILogicFactory::Lock(m_impl->logicFactory), archive, fileName, error);
		return [this
			, executor = std::move(executor)
			, reader = std::move(reader)
			, fileName = std::move(fileName)
			, callback = std::move(callback)
			, temporaryDir = std::move(temporaryDir)
			, error(std::move(error))
		] (size_t) mutable
		{
			const ScopedCall callbackGuard([&, callback = std::move(callback)] () mutable { callback(); });

			if (!temporaryDir)
				return m_impl->uiFactory->ShowError(error);

			if (reader == DEFAULT)
				return (void)QDesktopServices::openUrl(fileName);

			new ReaderProcess(reader, fileName, std::move(temporaryDir), m_impl->uiFactory->GetParentObject());
		};
	} });
}
