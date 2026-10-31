#include "BooksExtractor.h"

#include <filesystem>
#include <ranges>

#include <QBuffer>
#include <QTemporaryDir>

#include "fnd/StrUtil.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/ExportStat.h"
#include "interface/constants/ModelRole.h"
#include "interface/constants/ObjectConnectionID.h"
#include "interface/constants/SettingsConstant.h"

#include "data/DataItem.h"
#include "platform/StrUtil.h"
#include "util/FunctorExecutionForwarder.h"
#include "util/IExecutor.h"
#include "util/ImageRestore.h"
#include "util/ObjectsConnector.h"

#include "log.h"
#include "zip.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace {

constexpr auto EXPORT_ENCODING_KEY = "Preferences/Export/Encoding/export";
constexpr auto AUTO_RATE_KEY       = "Preferences/Export/AutoRateOnExport";

class IExportHelper // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IExportHelper()                                                                                             = default;
	virtual void                                       CheckPath(std::filesystem::path& path, const std::string& suffix) = 0;
	virtual std::unique_ptr<const Util::ExtractedBook> GetMetadataReplacement(const Util::ExtractedBook& book) const     = 0;
};

void Write(const QByteArray& input, const std::filesystem::path& path)
{
	QFile output(Platform::PathToString(path));
	if (!output.open(QIODevice::WriteOnly))
		throw std::ios_base::failure(std::format("Cannot open '{}' for write", path));

	if (output.write(input) != input.size())
		throw std::ios_base::failure(std::format("An error occurred while saving '{}'", path));
}

void Archive(const QByteArray& input, const std::filesystem::path& path, QString fileName, std::shared_ptr<Zip::ProgressCallback> zipProgressCallback)
{
	Zip  zip(Platform::PathToString(path), Zip::Format::Zip, false, std::move(zipProgressCallback));
	auto zipFiles = Zip::CreateZipFileController();
	zipFiles->AddFile(std::move(fileName), input, QDateTime::currentDateTime());
	zip.Write(*zipFiles);
}

void Unpack(QByteArray& input, const std::filesystem::path& path)
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
			return;
		}

		std::ranges::for_each(zip.GetFileNameList(), [&](const auto& fileName) {
			const auto fullPath = folder / Platform::StringToPath(fileName);
			std::filesystem::create_directories(fullPath.parent_path());
			Write(zip.Read(fileName)->GetStream().readAll(), fullPath);
		});
	}
	catch (const std::exception& ex)
	{
		PLOGE << ex.what();
	}

	Write(input, path);
}

enum class WriteMode
{
	AsIs,
	Archive,
	Unpack,
};

std::filesystem::path Write(
	const ISettings&                       settings,
	QIODevice&                             input,
	const QString&                         folder,
	const Util::ExtractedBook&             book,
	IProgressController::IProgressItem&    progress,
	std::shared_ptr<Zip::ProgressCallback> zipProgressCallback,
	QString                                encoding,
	IExportHelper&                         exportHelper,
	const WriteMode                        mode
)
{
	const QFileInfo dstFileInfo(book.dstFileName);
	if (const auto dstDir = dstFileInfo.dir(); !(dstDir.exists() || dstDir.mkpath(".")))
		throw std::ios_base::failure(std::format("Cannot create folder '{}'", dstDir.path()));

	auto dstFilePath = Platform::StringToPath(QDir::toNativeSeparators(book.dstFileName));

	const auto suffix = mode == WriteMode::Archive ? ".zip" : "";
	exportHelper.CheckPath(dstFilePath, suffix);
	dstFilePath.concat(suffix);

	if (exists(dstFilePath))
		if (!remove(dstFilePath))
			throw std::ios_base::failure(std::format("Cannot remove existing file '{}'", dstFilePath));

	[&] {
		auto bytes = PrepareToExport(input, folder, book.file, settings, std::move(encoding), exportHelper.GetMetadataReplacement(book));
		switch (mode)
		{
			case WriteMode::AsIs:
				return Write(bytes, dstFilePath);
			case WriteMode::Archive:
				return Archive(bytes, dstFilePath, dstFileInfo.completeBaseName() + "." + QFileInfo(book.file).suffix(), std::move(zipProgressCallback));
			case WriteMode::Unpack:
				return Unpack(bytes, dstFilePath);
			default: // NOLINT(clang-diagnostic-covered-switch-default)
				return assert(false && "unexpected mode");
		}
	}();

	if (!exists(dstFilePath))
		throw std::ios_base::failure(std::format("Save to '{}' failed", dstFilePath));

	progress.Increment(1);

	return dstFilePath;
}

std::filesystem::path Process(
	const ISettings&                       settings,
	const std::filesystem::path&           archiveFolder,
	const Util::ExtractedBook&             book,
	IProgressController::IProgressItem&    progress,
	std::shared_ptr<Zip::ProgressCallback> zipProgressCallback,
	QString                                encoding,
	IExportHelper&                         exportHelper,
	const WriteMode                        mode
)
{
	if (progress.IsStopped())
		return {};

	const auto folder = QDir::fromNativeSeparators(Platform::PathToString(archiveFolder / Platform::StringToPath(book.folder)));
	if (!QFile::exists(folder))
		throw std::runtime_error((folder + ": archive not found").toStdString());

	const Zip  zip(folder);
	const auto stream = zip.Read(book.file);
	return Write(settings, stream->GetStream(), folder, book, progress, std::move(zipProgressCallback), std::move(encoding), exportHelper, mode);
}

bool Process(
	const ISettings&                    settings,
	const std::filesystem::path&        archiveFolder,
	const QString&                      dstFolder,
	DB::IDatabase&                      db,
	const Util::ExtractedBook&          book,
	IProgressController::IProgressItem& progress,
	QString                             encoding,
	IExportHelper&                      exportHelper,
	const IScriptController&            scriptController,
	IScriptController::Commands         commands
)
{
	const auto needFile   = std::ranges::any_of(commands, [](const auto& command) {
		return IScriptController::HasMacro(command.args, IScriptController::Macro::SourceFile);
	});
	const auto sourceFile = needFile ? Process(settings, archiveFolder, book, progress, {}, std::move(encoding), exportHelper, WriteMode::AsIs) : std::filesystem::path {};

	std::ranges::sort(commands, {}, [](const IScriptController::Command& command) {
		return command.number;
	});
	for (auto command : commands)
	{
		if (needFile)
			IScriptController::SetMacro(command.args, IScriptController::Macro::SourceFile, QDir::toNativeSeparators(Platform::PathToString(sourceFile)));
		IScriptController::SetMacro(command.args, IScriptController::Macro::UserDestinationFolder, QDir::toNativeSeparators(dstFolder));
		ILogicFactory::FillScriptTemplate(db, command.args, book);

		if (progress.IsStopped() || !scriptController.Execute(command))
			return false;
	}

	return true;
}

using ProcessFunctor = std::function<
	bool(const std::filesystem::path& archiveFolder, const QString& dstFolder, const Util::ExtractedBook& book, QString encoding, IProgressController::IProgressItem& progress, IExportHelper& exportHelper)>;

} // namespace

class BooksExtractor::Impl final
    : virtual IExportHelper
    , IProgressController::IObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(
		const BooksExtractor&                       self,
		std::shared_ptr<const ISettings>            settings,
		std::shared_ptr<ICollectionController>      collectionController,
		std::shared_ptr<IProgressController>        progressController,
		const std::shared_ptr<const ILogicFactory>& logicFactory,
		std::shared_ptr<const IScriptController>    scriptController,
		std::shared_ptr<const IBookExtractor>       bookExtractor,
		std::shared_ptr<const IDatabaseProvider>    databaseProvider
	)
		: m_self { self }
		, m_settings { std::move(settings) }
		, m_encoding { m_settings->Get(EXPORT_ENCODING_KEY, QString {}) }
		, m_autoRateValue { m_settings->Get(AUTO_RATE_KEY) }
		, m_collectionController { std::move(collectionController) }
		, m_progressController { std::move(progressController) }
		, m_logicFactory { logicFactory }
		, m_scriptController { std::move(scriptController) }
		, m_bookExtractor { std::move(bookExtractor) }
		, m_databaseProvider { std::move(databaseProvider) }
		, m_needReplaceMetadata { m_settings->Get(Constant::Settings::EXPORT_REPLACE_METADATA_KEY, false) }
	{
		m_progressController->RegisterObserver(this);
	}

	~Impl() override = default;

	void Extract(QString dstFolder, Util::ExtractedBooks&& books, Callback callback, const ExportStat::Type exportStatType, ProcessFunctor processFunctor)
	{
		assert(!m_callback);
		m_hasError       = false;
		m_callback       = std::move(callback);
		m_taskCount      = std::size(books);
		m_exportStatType = static_cast<int>(exportStatType);
		m_processFunctor = std::move(processFunctor);
		ILogicFactory::Lock(m_logicFactory)->GetExecutor({ static_cast<int>(m_taskCount) }).swap(m_executor);
		m_dstFolder     = std::move(dstFolder);
		m_archiveFolder = Platform::StringToPath(m_collectionController->GetActiveCollection().GetFolder());

		std::for_each(std::make_move_iterator(std::begin(books)), std::make_move_iterator(std::end(books)), [&](Util::ExtractedBook&& book) {
			(*m_executor)(CreateTask(std::move(book)));
		});
	}

	std::shared_ptr<const IScriptController> GetScriptController() const
	{
		return m_scriptController;
	}

	std::shared_ptr<Zip::ProgressCallback> GetZipProgressCallback()
	{
		return m_logicFactory.lock()->CreateZipProgressCallback(m_progressController);
	}

	const ISettings& GetSettings() const
	{
		return *m_settings;
	}

	std::shared_ptr<const ILogicFactory> GetLogicFactory() const
	{
		return ILogicFactory::Lock(m_logicFactory);
	}

	std::shared_ptr<const IDatabaseProvider> GetDatabaseUser() const
	{
		return m_databaseProvider;
	}

private: // IExportHelper
	void CheckPath(std::filesystem::path& path, const std::string& suffix) override
	{
		std::lock_guard lock(m_usedPathGuard);

		const auto addSuffix = [&](std::filesystem::path src) {
			src.concat(suffix);
			src.make_preferred();
			return Platform::PathToString(src).toLower();
		};

		std::ranges::transform(
			std::filesystem::directory_iterator(path.parent_path()) | std::views::filter([](const auto& item) {
				return item.is_regular_file();
			}),
			std::inserter(m_usedPath, m_usedPath.end()),
			[&](const auto& item) {
				return Platform::PathToString(item.path()).toLower();
			}
		);

		if (m_usedPath.emplace(addSuffix(path)).second)
			return;

		const auto folder   = path.parent_path();
		const auto basePath = path.stem().wstring();
		const auto ext      = path.extension().wstring();
		for (int i = 1;; ++i)
		{
			path = folder / (basePath + std::to_wstring(i).append(ext));
			if (m_usedPath.emplace(addSuffix(path)).second)
				return;
		}
	}

	std::unique_ptr<const Util::ExtractedBook> GetMetadataReplacement(const Util::ExtractedBook& book) const override
	{
		if (!m_needReplaceMetadata)
			return {};

		auto metadataReplacement = std::make_unique<Util::ExtractedBook>(book);
		if (book.authorFull.firstName.isEmpty() && book.authorFull.middleName.isEmpty() && book.authorFull.lastName.isEmpty())
			metadataReplacement->authorFull = m_bookExtractor->GetExtractedBookAuthor(QString::number(book.id));

		return metadataReplacement;
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
	Util::IExecutor::Task CreateTask(Util::ExtractedBook&& book)
	{
		std::shared_ptr progressItem = m_progressController->Add(1);
		auto            taskName     = book.file.toStdString();
		return Util::IExecutor::Task { std::move(taskName), [this, book = std::move(book), progressItem = std::move(progressItem)]() mutable {
										  bool error = false;
										  try
										  {
											  if (m_processFunctor(m_archiveFolder, m_dstFolder, book, m_encoding, *progressItem, *this))
												  OnExtracted(book.id);
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

	void OnExtracted(const long long id) const
	{
		const auto db          = m_databaseProvider->GetDatabaseWritable();
		const auto transaction = db->CreateTransaction();
		transaction->CreateCommand(ExportStat::INSERT_QUERY)->Execute(id, m_exportStatType);
		if (m_autoRateValue.isValid())
		{
			transaction->CreateCommand(IDatabaseProvider::SET_USER_RATE_COMMAND)->Execute(id, m_autoRateValue.toInt());
			m_forwarder.Forward([this, id] {
				emit m_self.SetUserRate({ .ids = { id }, .column = BookItem::Column::UserRate, .value = m_autoRateValue.toString() });
			});
		}
		transaction->Commit();
	}

private:
	const BooksExtractor&                                     m_self;
	std::shared_ptr<const ISettings>                          m_settings;
	const QString                                             m_encoding;
	const QVariant                                            m_autoRateValue;
	PropagateConstPtr<ICollectionController, std::shared_ptr> m_collectionController;
	PropagateConstPtr<IProgressController, std::shared_ptr>   m_progressController;
	std::weak_ptr<const ILogicFactory>                        m_logicFactory;
	std::shared_ptr<const IScriptController>                  m_scriptController;
	std::shared_ptr<const IBookExtractor>                     m_bookExtractor;
	std::shared_ptr<const IDatabaseProvider>                  m_databaseProvider;
	Callback                                                  m_callback;
	size_t                                                    m_taskCount { 0 };
	bool                                                      m_hasError { false };
	int                                                       m_exportStatType;
	ProcessFunctor                                            m_processFunctor { nullptr };
	std::unique_ptr<Util::IExecutor>                          m_executor;
	QString                                                   m_dstFolder;
	std::filesystem::path                                     m_archiveFolder;
	std::mutex                                                m_usedPathGuard;
	std::unordered_set<QString>                               m_usedPath;
	bool                                                      m_needReplaceMetadata;
	Util::FunctorExecutionForwarder                           m_forwarder;
};

BooksExtractor::BooksExtractor(
	std::shared_ptr<const ISettings>            settings,
	std::shared_ptr<ICollectionController>      collectionController,
	std::shared_ptr<IMainProgressController>    progressController,
	const std::shared_ptr<const ILogicFactory>& logicFactory,
	std::shared_ptr<const IScriptController>    scriptController,
	std::shared_ptr<const IBookExtractor>       bookExtractor,
	std::shared_ptr<const IDatabaseProvider>    databaseProvider
)
	: m_impl(*this, std::move(settings), std::move(collectionController), std::move(progressController), logicFactory, std::move(scriptController), std::move(bookExtractor), std::move(databaseProvider))
{
	Util::ObjectsConnector::registerEmitter(ObjectConnectorID::SET_BOOKS_COLUMN_VALUE, this, SIGNAL(SetUserRate(const SetForIdsArg&)));
	PLOGV << "BooksExtractor created";
}

BooksExtractor::~BooksExtractor()
{
	PLOGV << "BooksExtractor destroyed";
}

void BooksExtractor::ExtractAsArchives(QString folder, const QString& /*parameter*/, Util::ExtractedBooks&& books, Callback callback)
{
	auto zipProgressCallback = m_impl->GetZipProgressCallback();
	m_impl->Extract(
		std::move(folder),
		std::move(books),
		std::move(callback),
		ExportStat::Type::Archive,
		[this, zipProgressCallback = std::move(zipProgressCallback)](
			const std::filesystem::path& archiveFolder,
			const QString& /*dstFolder*/,
			const Util::ExtractedBook&          book,
			QString                             encoding,
			IProgressController::IProgressItem& progress,
			IExportHelper&                      exportHelper
		) mutable {
			return !Process(m_impl->GetSettings(), archiveFolder, book, progress, std::move(zipProgressCallback), std::move(encoding), exportHelper, WriteMode::Archive).empty();
		}
	);
}

void BooksExtractor::ExtractAsIs(QString folder, const QString& /*parameter*/, Util::ExtractedBooks&& books, Callback callback)
{
	m_impl->Extract(
		std::move(folder),
		std::move(books),
		std::move(callback),
		ExportStat::Type::AsIs,
		[this](
			const std::filesystem::path& archiveFolder,
			const QString& /*dstFolder*/,
			const Util::ExtractedBook&          book,
			QString                             encoding,
			IProgressController::IProgressItem& progress,
			IExportHelper&                      exportHelper
		) {
			return !Process(m_impl->GetSettings(), archiveFolder, book, progress, {}, std::move(encoding), exportHelper, WriteMode::AsIs).empty();
		}
	);
}

void BooksExtractor::ExtractUnpack(QString folder, const QString& /*parameter*/, Util::ExtractedBooks&& books, Callback callback)
{
	m_impl->Extract(
		std::move(folder),
		std::move(books),
		std::move(callback),
		ExportStat::Type::Unpack,
		[this](
			const std::filesystem::path& archiveFolder,
			const QString& /*dstFolder*/,
			const Util::ExtractedBook&          book,
			QString                             encoding,
			IProgressController::IProgressItem& progress,
			IExportHelper&                      exportHelper
		) {
			return !Process(m_impl->GetSettings(), archiveFolder, book, progress, {}, std::move(encoding), exportHelper, WriteMode::Unpack).empty();
		}
	);
}

void BooksExtractor::ExtractAsScript(QString folder, const QString& parameter, Util::ExtractedBooks&& books, Callback callback)
{
	auto scriptController = m_impl->GetScriptController();
	auto commands         = scriptController->GetCommands(parameter);
	m_impl->Extract(
		std::move(folder),
		std::move(books),
		std::move(callback),
		ExportStat::Type::Script,
		[this, scriptController = std::move(scriptController), commands = std::move(commands), dbProvider = m_impl->GetDatabaseUser()](
			const std::filesystem::path&        archiveFolder,
			const QString&                      dstFolder,
			const Util::ExtractedBook&          book,
			QString                             encoding,
			IProgressController::IProgressItem& progress,
			IExportHelper&                      exportHelper
		) {
			const auto db = dbProvider->GetDatabase();
			return Process(m_impl->GetSettings(), archiveFolder, dstFolder, *db, book, progress, std::move(encoding), exportHelper, *scriptController, commands);
		}
	);
}
