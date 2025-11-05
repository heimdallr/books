#include "BooksExtractor.h"

#include <filesystem>

#include <QBuffer>
#include <QTemporaryDir>

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/ExportStat.h"

#include "Util/IExecutor.h"
#include "shared/ImageRestore.h"

#include "log.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

class IPathChecker // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IPathChecker()                         = default;
	virtual void Check(std::filesystem::path& path) = 0;
};

bool Write(const QByteArray& input, const std::filesystem::path& path)
{
	QFile output(QString::fromStdWString(path));
	return output.open(QIODevice::WriteOnly) && output.write(input) == input.size();
}

bool Archive(QByteArray input, const std::filesystem::path& path, QString fileName, std::shared_ptr<Zip::ProgressCallback> zipProgressCallback)
{
	try
	{
		Zip  zip(QString::fromStdWString(path), Zip::Format::Zip, false, std::move(zipProgressCallback));
		auto zipFiles = Zip::CreateZipFileController();
		zipFiles->AddFile(std::move(fileName), std::move(input), QDateTime::currentDateTime());
		zip.Write(std::move(zipFiles));
		return true;
	}
	catch (const std::exception& ex)
	{
		PLOGE << ex.what();
	}
	return false;
}

bool Unpack(QByteArray& input, const std::filesystem::path& path)
{
	try
	{
		QBuffer buffer(&input);
		buffer.open(QIODevice::ReadOnly);
		Zip zip(buffer);

		std::filesystem::path folder = path;
		folder.replace_extension("");
		std::error_code ec;
		if (!create_directories(folder, ec))
		{
			PLOGE << ec.message() << " (" << ec.value() << ")";
			return false;
		}

		return std::ranges::all_of(zip.GetFileNameList(), [&](const auto& fileName) {
			const auto fullPath = folder / fileName.toStdWString();
			std::filesystem::create_directories(fullPath.parent_path());
			QFile      output(QString::fromStdWString(fullPath));
			const auto fileSize = zip.GetFileSize(fileName);
			return output.open(QIODevice::WriteOnly) && output.write(zip.Read(fileName)->GetStream().readAll()) == static_cast<qint64>(fileSize);
		});
	}
	catch (const std::exception& ex)
	{
		PLOGE << ex.what();
	}

	return Write(input, path);
}

enum class WriteMode
{
	AsIs,
	Archive,
	Unpack,
};

std::pair<bool, std::filesystem::path> Write(
	const std::shared_ptr<const ISettings>& settings,
	QIODevice&                              input,
	const QString&                          dstFileName,
	const QString&                          folder,
	const ILogicFactory::ExtractedBook&     book,
	IProgressController::IProgressItem&     progress,
	std::shared_ptr<Zip::ProgressCallback>  zipProgressCallback,
	IPathChecker&                           pathChecker,
	const WriteMode                         mode
)
{
	auto            result = std::make_pair(false, std::filesystem::path {});
	const QFileInfo dstFileInfo(dstFileName);
	if (const auto dstDir = dstFileInfo.absolutePath(); !(QDir().exists(dstDir) || QDir().mkpath(dstDir)))
		return result;

	result.second = QDir::toNativeSeparators(dstFileName).toStdWString();
	if (mode == WriteMode::Archive)
		result.second.replace_extension(".zip");

	pathChecker.Check(result.second);

	if (exists(result.second))
		if (!remove(result.second))
			return assert(false), result;

	result.first = [&] {
		auto bytes = RestoreImages(input, folder, book.file, settings);
		switch (mode)
		{
			case WriteMode::AsIs:
				return Write(bytes, result.second);
			case WriteMode::Archive:
				return Archive(std::move(bytes), result.second, dstFileInfo.fileName(), std::move(zipProgressCallback));
			case WriteMode::Unpack:
				return Unpack(bytes, result.second);
			default: // NOLINT(clang-diagnostic-covered-switch-default)
				return assert(false && "unexpected mode"), false;
		}
	}();
	progress.Increment(1);

	return result;
}

std::filesystem::path Process(
	const std::shared_ptr<const ISettings>& settings,
	const std::filesystem::path&            archiveFolder,
	const QString&                          dstFolder,
	DB::IDatabase&                          db,
	const ILogicFactory::ExtractedBook&     book,
	QString                                 outputFileTemplate,
	const IFillTemplateConverter&           converter,
	IProgressController::IProgressItem&     progress,
	std::shared_ptr<Zip::ProgressCallback>  zipProgressCallback,
	IPathChecker&                           pathChecker,
	const WriteMode                         mode
)
{
	if (progress.IsStopped())
		return {};

	converter.Fill(db, outputFileTemplate, book, dstFolder);

	const auto folder = QDir::fromNativeSeparators(QString::fromStdWString(archiveFolder / book.folder.toStdWString()));
	if (!QFile::exists(folder))
		throw std::runtime_error((folder + ": archive not found").toStdString());

	const Zip  zip(folder);
	const auto stream = zip.Read(book.file);
	auto [ok, path]   = Write(settings, stream->GetStream(), outputFileTemplate, folder, book, progress, std::move(zipProgressCallback), pathChecker, mode);
	if (!ok && exists(path))
		remove(path);

	return ok ? path : std::filesystem::path {};
}

void Process(
	const std::shared_ptr<const ISettings>& settings,
	const std::filesystem::path&            archiveFolder,
	const QString&                          dstFolder,
	DB::IDatabase&                          db,
	const ILogicFactory::ExtractedBook&     book,
	const QString&                          outputFileTemplate,
	const IFillTemplateConverter&           converter,
	IProgressController::IProgressItem&     progress,
	IPathChecker&                           pathChecker,
	const IScriptController&                scriptController,
	IScriptController::Commands             commands,
	const QTemporaryDir&                    tempDir
)
{
	const auto needFile   = std::ranges::any_of(commands, [](const auto& command) {
        return IScriptController::HasMacro(command.args, IScriptController::Macro::SourceFile);
    });
	const auto sourceFile = needFile ? Process(settings, archiveFolder, tempDir.filePath(""), db, book, outputFileTemplate, converter, progress, {}, pathChecker, WriteMode::AsIs) : std::filesystem::path {};

	std::ranges::sort(commands, {}, [](const IScriptController::Command& command) {
		return command.number;
	});
	for (auto command : commands)
	{
		if (needFile)
			IScriptController::SetMacro(command.args, IScriptController::Macro::SourceFile, QDir::toNativeSeparators(QString::fromStdWString(sourceFile)));
		IScriptController::SetMacro(command.args, IScriptController::Macro::UserDestinationFolder, QDir::toNativeSeparators(dstFolder));
		ILogicFactory::FillScriptTemplate(db, command.args, book);

		if (progress.IsStopped() || !scriptController.Execute(command))
			return;
	}
}

using ProcessFunctor = std::function<
	void(const std::filesystem::path& archiveFolder, const QString& dstFolder, const ILogicFactory::ExtractedBook& book, IProgressController::IProgressItem& progress, IPathChecker& pathChecker)>;
} // namespace

class BooksExtractor::Impl final
	: virtual IPathChecker
	, IProgressController::IObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(
		std::shared_ptr<const ISettings>            settings,
		std::shared_ptr<ICollectionController>      collectionController,
		std::shared_ptr<IProgressController>        progressController,
		const std::shared_ptr<const ILogicFactory>& logicFactory,
		std::shared_ptr<const IScriptController>    scriptController,
		std::shared_ptr<const IDatabaseUser>        databaseUser
	)
		: m_settings { std::move(settings) }
		, m_collectionController { std::move(collectionController) }
		, m_progressController { std::move(progressController) }
		, m_logicFactory { logicFactory }
		, m_scriptController { std::move(scriptController) }
		, m_databaseUser { std::move(databaseUser) }
	{
		m_progressController->RegisterObserver(this);
	}

	~Impl() override
	{
		m_progressController->UnregisterObserver(this);
	}

	void Extract(QString dstFolder, ILogicFactory::ExtractedBooks&& books, Callback callback, const ExportStat::Type exportStatType, ProcessFunctor processFunctor)
	{
		assert(!m_callback);
		m_hasError       = false;
		m_callback       = std::move(callback);
		m_taskCount      = std::size(books);
		m_processFunctor = std::move(processFunctor);
		ILogicFactory::Lock(m_logicFactory)->GetExecutor({ static_cast<int>(m_taskCount) }).swap(m_executor);
		m_dstFolder     = std::move(dstFolder);
		m_archiveFolder = m_collectionController->GetActiveCollection().GetFolder().toStdWString();

		const auto transaction = m_databaseUser->Database()->CreateTransaction();
		const auto command     = transaction->CreateCommand(ExportStat::INSERT_QUERY);

		std::for_each(std::make_move_iterator(std::begin(books)), std::make_move_iterator(std::end(books)), [&](ILogicFactory::ExtractedBook&& book) {
			command->Bind(0, book.id);
			command->Bind(1, static_cast<int>(exportStatType));
			command->Execute();

			(*m_executor)(CreateTask(std::move(book)));
		});

		transaction->Commit();
	}

	std::shared_ptr<const IScriptController> GetScriptController() const
	{
		return m_scriptController;
	}

	std::shared_ptr<Zip::ProgressCallback> GetZipProgressCallback()
	{
		return m_logicFactory.lock()->CreateZipProgressCallback(m_progressController);
	}

	std::shared_ptr<const ISettings> GetSettings() const
	{
		return m_settings;
	}

	std::shared_ptr<DB::IDatabase> GetDatabase() const
	{
		return m_databaseUser->Database();
	}

	std::shared_ptr<const ILogicFactory> GetLogicFactory() const
	{
		return ILogicFactory::Lock(m_logicFactory);
	}

private: // IPathChecker
	void Check(std::filesystem::path& path) override
	{
		std::lock_guard lock(m_usedPathGuard);
		if (m_usedPath.emplace(QString::fromStdWString(path).toLower()).second)
			return;

		const auto folder   = path.parent_path();
		const auto basePath = path.stem().wstring();
		const auto ext      = path.extension().wstring();
		for (int i = 1;; ++i)
		{
			path = folder / (basePath + std::to_wstring(i).append(ext));
			path.make_preferred();
			if (m_usedPath.emplace(QString::fromStdWString(path).toLower()).second)
				return;
		}
	}

private: // IProgressController::IObserver
	void OnStartedChanged() override
	{
	}

	void OnValueChanged() override
	{
	}

	void OnStop() override
	{
		m_executor->Stop();
		m_executor.reset();
	}

private:
	Util::IExecutor::Task CreateTask(ILogicFactory::ExtractedBook&& book)
	{
		std::shared_ptr progressItem = m_progressController->Add(1);
		auto            taskName     = book.file.toStdString();
		return Util::IExecutor::Task { std::move(taskName), [this, book = std::move(book), progressItem = std::move(progressItem)]() mutable {
										  bool error = false;
										  try
										  {
											  m_processFunctor(m_archiveFolder, m_dstFolder, book, *progressItem, *this);
										  }
										  catch (const std::exception& ex)
										  {
											  PLOGE << ex.what();
											  error = true;
										  }
										  return [this, error, progressItem = std::move(progressItem)](const size_t) {
											  m_hasError = error || m_hasError;
											  assert(m_taskCount > 0);
											  if (--m_taskCount == 0)
												  m_callback(m_hasError);
										  };
									  } };
	}

private:
	std::shared_ptr<const ISettings>                          m_settings;
	PropagateConstPtr<ICollectionController, std::shared_ptr> m_collectionController;
	PropagateConstPtr<IProgressController, std::shared_ptr>   m_progressController;
	std::weak_ptr<const ILogicFactory>                        m_logicFactory;
	std::shared_ptr<const IScriptController>                  m_scriptController;
	std::shared_ptr<const IDatabaseUser>                      m_databaseUser;
	Callback                                                  m_callback;
	size_t                                                    m_taskCount { 0 };
	bool                                                      m_hasError { false };
	ProcessFunctor                                            m_processFunctor { nullptr };
	std::unique_ptr<Util::IExecutor>                          m_executor;
	QString                                                   m_dstFolder;
	std::filesystem::path                                     m_archiveFolder;
	std::mutex                                                m_usedPathGuard;
	std::unordered_set<QString>                               m_usedPath;
};

BooksExtractor::BooksExtractor(
	std::shared_ptr<const ISettings>                   settings,
	std::shared_ptr<ICollectionController>             collectionController,
	std::shared_ptr<IBooksExtractorProgressController> progressController,
	const std::shared_ptr<const ILogicFactory>&        logicFactory,
	std::shared_ptr<const IScriptController>           scriptController,
	std::shared_ptr<IDatabaseUser>                     databaseUser
)
	: m_impl(std::move(settings), std::move(collectionController), std::move(progressController), logicFactory, std::move(scriptController), std::move(databaseUser))
{
	PLOGV << "BooksExtractor created";
}

BooksExtractor::~BooksExtractor()
{
	PLOGV << "BooksExtractor destroyed";
}

void BooksExtractor::ExtractAsArchives(QString folder, const QString& /*parameter*/, ILogicFactory::ExtractedBooks&& books, QString outputFileNameTemplate, Callback callback)
{
	auto zipProgressCallback = m_impl->GetZipProgressCallback();
	m_impl->Extract(
		std::move(folder),
		std::move(books),
		std::move(callback),
		ExportStat::Type::Archive,
		[outputFileNameTemplate = std::move(outputFileNameTemplate),
	     converter              = m_impl->GetLogicFactory()->CreateFillTemplateConverter(),
	     zipProgressCallback    = std::move(zipProgressCallback),
	     settings               = m_impl->GetSettings(),
	     db                     = m_impl->GetDatabase(
         )](const std::filesystem::path& archiveFolder, const QString& dstFolder, const ILogicFactory::ExtractedBook& book, IProgressController::IProgressItem& progress, IPathChecker& pathChecker) mutable {
			Process(settings, archiveFolder, dstFolder, *db, book, outputFileNameTemplate, *converter, progress, std::move(zipProgressCallback), pathChecker, WriteMode::Archive);
		}
	);
}

void BooksExtractor::ExtractAsIs(QString folder, const QString& /*parameter*/, ILogicFactory::ExtractedBooks&& books, QString outputFileNameTemplate, Callback callback)
{
	m_impl->Extract(
		std::move(folder),
		std::move(books),
		std::move(callback),
		ExportStat::Type::AsIs,
		[outputFileNameTemplate = std::move(outputFileNameTemplate),
	     converter              = m_impl->GetLogicFactory()->CreateFillTemplateConverter(),
	     settings               = m_impl->GetSettings(),
	     db                     = m_impl->GetDatabase(
         )](const std::filesystem::path& archiveFolder, const QString& dstFolder, const ILogicFactory::ExtractedBook& book, IProgressController::IProgressItem& progress, IPathChecker& pathChecker) {
			Process(settings, archiveFolder, dstFolder, *db, book, outputFileNameTemplate, *converter, progress, {}, pathChecker, WriteMode::AsIs);
		}
	);
}

void BooksExtractor::ExtractUnpack(QString folder, const QString& /*parameter*/, ILogicFactory::ExtractedBooks&& books, QString outputFileNameTemplate, Callback callback)
{
	m_impl->Extract(
		std::move(folder),
		std::move(books),
		std::move(callback),
		ExportStat::Type::Unpack,
		[outputFileNameTemplate = std::move(outputFileNameTemplate),
	     converter              = m_impl->GetLogicFactory()->CreateFillTemplateConverter(),
	     settings               = m_impl->GetSettings(),
	     db                     = m_impl->GetDatabase(
         )](const std::filesystem::path& archiveFolder, const QString& dstFolder, const ILogicFactory::ExtractedBook& book, IProgressController::IProgressItem& progress, IPathChecker& pathChecker) {
			Process(settings, archiveFolder, dstFolder, *db, book, outputFileNameTemplate, *converter, progress, {}, pathChecker, WriteMode::Unpack);
		}
	);
}

void BooksExtractor::ExtractAsScript(QString folder, const QString& parameter, ILogicFactory::ExtractedBooks&& books, QString outputFileNameTemplate, Callback callback)
{
	auto scriptController = m_impl->GetScriptController();
	auto commands         = scriptController->GetCommands(parameter);
	auto converter        = m_impl->GetLogicFactory()->CreateFillTemplateConverter(true);
	m_impl->Extract(
		std::move(folder),
		std::move(books),
		std::move(callback),
		ExportStat::Type::Script,
		[scriptController       = std::move(scriptController),
	     commands               = std::move(commands),
	     converter              = std::move(converter),
	     tempDir                = std::make_shared<QTemporaryDir>(),
	     outputFileNameTemplate = std::move(outputFileNameTemplate),
	     settings               = m_impl->GetSettings(),
	     db                     = m_impl->GetDatabase(
         )](const std::filesystem::path& archiveFolder, const QString& dstFolder, const ILogicFactory::ExtractedBook& book, IProgressController::IProgressItem& progress, IPathChecker& pathChecker) {
			Process(settings, archiveFolder, dstFolder, *db, book, outputFileNameTemplate, *converter, progress, pathChecker, *scriptController, commands, *tempDir);
		}
	);
}
