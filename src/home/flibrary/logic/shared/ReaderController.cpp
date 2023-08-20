#include "ReaderController.h"

#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include <quazip>
#include <plog/Log.h>

#include "util/IExecutor.h"

#include "interface/constants/Localization.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/ui/IUiFactory.h"
#include "util/ISettings.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto CONTEXT = "ReaderController";
constexpr auto DIALOG_TITLE = QT_TRANSLATE_NOOP("ReaderController", "Select %1 reader");
constexpr auto DIALOG_FILTER = QT_TRANSLATE_NOOP("ReaderController", "Applications (*.exe)");

constexpr auto READER_KEY = "Reader/%1";

constexpr auto BUFFER_SIZE = 16 * 1024;

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

}

struct ReaderController::Impl
{
	std::shared_ptr<ISettings> settings;
	PropagateConstPtr<ICollectionController, std::shared_ptr> collectionController;
	PropagateConstPtr<ILogicFactory, std::shared_ptr> logicFactory;
	PropagateConstPtr<IUiFactory, std::shared_ptr> uiFactory;

	Impl(std::shared_ptr<ISettings> settings
		, std::shared_ptr<ICollectionController> collectionController
		, std::shared_ptr<ILogicFactory> logicFactory
		, std::shared_ptr<IUiFactory> uiFactory
	)
		: settings(std::move(settings))
		, collectionController(std::move(collectionController))
		, logicFactory(std::move(logicFactory))
		, uiFactory(std::move(uiFactory))
	{
	}
};

ReaderController::ReaderController(std::shared_ptr<ISettings> settings
	, std::shared_ptr<ICollectionController> collectionController
	, std::shared_ptr<ILogicFactory> logicFactory
	, std::shared_ptr<IUiFactory> uiFactory
)
	: m_impl(std::move(settings)
		, std::move(collectionController)
		, std::move(logicFactory)
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
	if (reader.isEmpty() && !(reader = m_impl->uiFactory->GetOpenFileName(Tr(DIALOG_TITLE).arg(ext), {}, Tr(DIALOG_FILTER))).isEmpty())
		m_impl->settings->Set(key, reader);

	if (reader.isEmpty())
		return;

	auto archive = QString("%1/%2").arg(m_impl->collectionController->GetActiveCollection()->folder, folderName);
	std::shared_ptr executor = m_impl->logicFactory->GetExecutor();
	(*executor)({ "Extract book", [&
		, executor
		, reader = std::move(reader)
		, archive = std::move(archive)
		, fileName = std::move(fileName)
		, callback = std::move(callback)
	] () mutable
	{
		auto temporaryDir = std::make_shared<QTemporaryDir>();
		QuaZip zip(archive);
		zip.open(QuaZip::Mode::mdUnzip);
		zip.setCurrentFile(fileName);

		QuaZipFile zipFile(&zip);
		zipFile.open(QIODevice::ReadOnly);
		fileName = temporaryDir->filePath(fileName);
		const std::unique_ptr<char[]> buffer(new char[BUFFER_SIZE]);
		if (QFile file(fileName); file.open(QIODevice::WriteOnly))
			while (const auto size = zipFile.read(buffer.get(), BUFFER_SIZE))
				file.write(buffer.get(), size);

		return [&
			, executor = std::move(executor)
			, reader = std::move(reader)
			, fileName = std::move(fileName)
			, callback = std::move(callback)
			, temporaryDir = std::move(temporaryDir)
		] (size_t) mutable
		{
			new ReaderProcess(reader, fileName, std::move(temporaryDir), m_impl->uiFactory->GetParentObject());
			callback();
		};
	} });
}
