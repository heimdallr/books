#include "BooksExtractor.h"

#include <filesystem>

#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>

#include "Util/IExecutor.h"

#include <plog/Log.h>

#include "interface/logic/ICollectionController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IProgressController.h"
#include "interface/logic/IScriptController.h"

#include "ImageRestore.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

class IPathChecker  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IPathChecker() = default;
	virtual void Check(std::filesystem::path & path) = 0;
};

bool Write(const QByteArray & input, const std::filesystem::path & path)
{
	QFile output(QString::fromStdWString(path));
	return true
		&& output.open(QIODevice::WriteOnly)
		&& output.write(input) == input.size();
}

bool Archive(const QByteArray & input, const std::filesystem::path & path, const QString & fileName)
{
	try
	{
		Zip zip(QString::fromStdWString(path), Zip::Format::Zip);
		auto & stream = zip.Write(fileName);
		stream.write(input);
		return true;
	}
	catch(const std::exception & ex)
	{
		PLOGE << ex.what();
	}
	return false;
}

QString RemoveIllegalCharacters(QString str)
{
	str.remove(QRegularExpression(R"([/\\<>:"\|\?\*\t])"));

	while (!str.isEmpty() && str.endsWith('.'))
		str.chop(1);

	if (str.isEmpty())
		str = "_";

	return str.simplified();
}

std::pair<bool, std::filesystem::path> Write(QIODevice & input
	, const QString & dstFileName
	, const QString & folder
	, const BooksExtractor::Book & book
	, IProgressController::IProgressItem & progress
	, IPathChecker & pathChecker
	, const bool archive
)
{
	auto result = std::make_pair(false, std::filesystem::path{});
	const QFileInfo dstFileInfo(dstFileName);
	if (const auto dstDir = dstFileInfo.absolutePath(); !(QDir().exists(dstDir) || QDir().mkpath(dstDir)))
		return result;

	result.second = QDir::toNativeSeparators(dstFileName).toStdWString();
	if (archive)
		result.second.replace_extension(".zip");

	pathChecker.Check(result.second);

	if (exists(result.second))
		if(!remove(result.second))
			return result;

	const auto extractedBytes = input.readAll();
	const auto bytes = RestoreImages(extractedBytes, folder, book.file);
	progress.Increment(extractedBytes.size());

	result.first = archive ? Archive(bytes, result.second, dstFileInfo.fileName()) : Write(bytes, result.second);

	return result;
}


std::filesystem::path Process(const std::filesystem::path & archiveFolder
	, const QString & dstFolder
	, const BooksExtractor::Book & book
	, QString outputFileTemplate
	, IProgressController::IProgressItem & progress
	, IPathChecker & pathChecker
	, const bool asArchives
)
{
	if (progress.IsStopped())
		return {};

	IScriptController::SetMacro(outputFileTemplate, IScriptController::Macro::UserDestinationFolder, dstFolder);
	BooksExtractor::FillScriptTemplate(outputFileTemplate, book);

	const auto folder = QDir::fromNativeSeparators(QString::fromStdWString(archiveFolder / book.folder.toStdWString()));
	const Zip zip(folder);
	auto [ok, path] = Write(zip.Read(book.file), outputFileTemplate, folder, book, progress, pathChecker, asArchives);
	if (!ok && exists(path))
		remove(path);

	return ok ? path : std::filesystem::path{};
}

void Process(const std::filesystem::path & archiveFolder
	, const QString & dstFolder
	, const BooksExtractor::Book & book
	, const QString & outputFileTemplate
	, IProgressController::IProgressItem & progress
	, IPathChecker & pathChecker
	, const IScriptController & scriptController
	, const IScriptController::Commands & commands
	, const QTemporaryDir & tempDir
)
{
	const auto sourceFile = Process(archiveFolder, tempDir.filePath(""), book, outputFileTemplate, progress, pathChecker, false);
	for (auto command : commands)
	{
		IScriptController::SetMacro(command.args, IScriptController::Macro::SourceFile, QDir::toNativeSeparators(QString::fromStdWString(sourceFile)));
		IScriptController::SetMacro(command.args, IScriptController::Macro::UserDestinationFolder, QDir::toNativeSeparators(dstFolder));
		BooksExtractor::FillScriptTemplate(command.args, book);

		if (false
			|| progress.IsStopped()
			|| !scriptController.Execute(command)
			)
			return;
	}
}

using ProcessFunctor = std::function<void(const std::filesystem::path & archiveFolder
	, const QString & dstFolder
	, const BooksExtractor::Book & book
	, IProgressController::IProgressItem & progress
	, IPathChecker & pathChecker
	)>;
}

class BooksExtractor::Impl final
	: virtual IPathChecker
	, IProgressController::IObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(std::shared_ptr<ICollectionController> collectionController
		, std::shared_ptr<IProgressController> progressController
		, std::shared_ptr<ILogicFactory> logicFactory
		, std::shared_ptr<const IScriptController> scriptController
	)
		: m_collectionController(std::move(collectionController))
		, m_progressController(std::move(progressController))
		, m_logicFactory(std::move(logicFactory))
		, m_scriptController(std::move(scriptController))
	{
		m_progressController->RegisterObserver(this);
	}

	~Impl() override
	{
		m_progressController->UnregisterObserver(this);
	}

	void Extract(QString dstFolder, Books && books, Callback callback, ProcessFunctor processFunctor)
	{
		assert(!m_callback);
		m_hasError = false;
		m_callback = std::move(callback);
		m_taskCount = std::size(books);
		m_processFunctor = std::move(processFunctor);
		m_logicFactory->GetExecutor({ static_cast<int>(m_taskCount)}).swap(m_executor);
		m_dstFolder = std::move(dstFolder);
		m_archiveFolder = m_collectionController->GetActiveCollection()->folder.toStdWString();

		std::for_each(std::make_move_iterator(std::begin(books)), std::make_move_iterator(std::end(books)), [&] (Book && book)
		{
			(*m_executor)(CreateTask(std::move(book)));
		});
	}

	std::shared_ptr<const IScriptController> GetScriptController() const
	{
		return m_scriptController;
	}

private: // IPathChecker
	void Check(std::filesystem::path& path) override
	{
		std::lock_guard lock(m_usedPathGuard);
		if (m_usedPath.insert(QString::fromStdWString(path).toLower()).second)
			return;

		const auto folder = path.parent_path();
		const auto basePath = path.stem().wstring();
		const auto ext = path.extension().wstring();
		for(int i = 1; ; ++i)
		{
			path = folder / (basePath + std::to_wstring(i).append(ext));
			path.make_preferred();
			if (m_usedPath.insert(QString::fromStdWString(path).toLower()).second)
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
		QTimer::singleShot(0, [&]
		{
			m_callback(m_hasError);
		});
	}

private:
	Util::IExecutor::Task CreateTask(Book && book)
	{
		std::shared_ptr progressItem = m_progressController->Add(book.size);
		auto taskName = book.file.toStdString();
		return Util::IExecutor::Task
		{
			std::move(taskName), [&, book = std::move(book), progressItem = std::move(progressItem)] ()
			{
				bool error = false;
				try
				{
					m_processFunctor(m_archiveFolder, m_dstFolder, book, *progressItem, *this);
				}
				catch(const std::exception & ex)
				{
					PLOGE << ex.what();
					error = true;
				}
				return [&, error] (const size_t)
				{
					m_hasError = error || m_hasError;
					assert(m_taskCount > 0);
					if (--m_taskCount == 0)
						m_callback(m_hasError);
				};
			}
		};
	}

private:
	PropagateConstPtr<ICollectionController, std::shared_ptr> m_collectionController;
	PropagateConstPtr<IProgressController, std::shared_ptr> m_progressController;
	PropagateConstPtr<ILogicFactory, std::shared_ptr> m_logicFactory;
	std::shared_ptr<const IScriptController> m_scriptController;
	Callback m_callback;
	size_t m_taskCount { 0 };
	bool m_hasError { false };
	ProcessFunctor m_processFunctor { nullptr };
	std::unique_ptr<Util::IExecutor> m_executor;
	QString m_dstFolder;
	std::filesystem::path m_archiveFolder;
	std::mutex m_usedPathGuard;
	std::unordered_set<QString> m_usedPath;
};

BooksExtractor::BooksExtractor(std::shared_ptr<ICollectionController> collectionController
	, std::shared_ptr<IBooksExtractorProgressController> progressController
	, std::shared_ptr<ILogicFactory> logicFactory
	, std::shared_ptr<const IScriptController> scriptController
)
	: m_impl(std::move(collectionController)
		, std::move(progressController)
		, std::move(logicFactory)
		, std::move(scriptController)
	)
{
	PLOGD << "BooksExtractor created";
}

BooksExtractor::~BooksExtractor()
{
	PLOGD << "BooksExtractor destroyed";
}

void BooksExtractor::ExtractAsArchives(QString folder, const QString &/*parameter*/, Books && books, QString outputFileNameTemplate, Callback callback)
{
	m_impl->Extract(std::move(folder), std::move(books), std::move(callback)
		, [outputFileNameTemplate = std::move(outputFileNameTemplate)] (const std::filesystem::path & archiveFolder
			, const QString & dstFolder
			, const Book & book
			, IProgressController::IProgressItem & progress
			, IPathChecker & pathChecker
			)
	{
		Process(archiveFolder, dstFolder, book, outputFileNameTemplate, progress, pathChecker, true);
	});
}

void BooksExtractor::ExtractAsIs(QString folder, const QString &/*parameter*/, Books && books, QString outputFileNameTemplate, Callback callback)
{
	m_impl->Extract(std::move(folder), std::move(books), std::move(callback)
		, [outputFileNameTemplate = std::move(outputFileNameTemplate)] (const std::filesystem::path & archiveFolder
			, const QString & dstFolder
			, const Book & book
			, IProgressController::IProgressItem & progress
			, IPathChecker & pathChecker
			)
	{
		Process(archiveFolder, dstFolder, book, outputFileNameTemplate, progress, pathChecker, false);
	});
}

void BooksExtractor::ExtractAsScript(QString folder, const QString &parameter, Books && books, QString outputFileNameTemplate, Callback callback)
{
	auto scriptController = m_impl->GetScriptController();
	auto commands = scriptController->GetCommands(parameter);
	m_impl->Extract(std::move(folder), std::move(books), std::move(callback)
		, [scriptController = std::move(scriptController)
			, commands = std::move(commands)
			, tempDir = std::make_shared<QTemporaryDir>()
			, outputFileNameTemplate = std::move(outputFileNameTemplate)
		]
		(const std::filesystem::path & archiveFolder
			, const QString & dstFolder
			, const Book & book
			, IProgressController::IProgressItem & progress
			, IPathChecker & pathChecker
			)
	{
		Process(archiveFolder, dstFolder, book, outputFileNameTemplate, progress, pathChecker, *scriptController, commands, *tempDir);
	});
}

void BooksExtractor::FillScriptTemplate(QString & scriptTemplate, const Book & book)
{
	const QFileInfo fileInfo(book.file);
	IScriptController::SetMacro(scriptTemplate, IScriptController::Macro::Title, RemoveIllegalCharacters(book.title));
	IScriptController::SetMacro(scriptTemplate, IScriptController::Macro::FileExt, RemoveIllegalCharacters(fileInfo.suffix()));
	IScriptController::SetMacro(scriptTemplate, IScriptController::Macro::FileName, RemoveIllegalCharacters(fileInfo.fileName()));
	IScriptController::SetMacro(scriptTemplate, IScriptController::Macro::BaseFileName, RemoveIllegalCharacters(fileInfo.completeBaseName()));
	IScriptController::SetMacro(scriptTemplate, IScriptController::Macro::Uid, QUuid::createUuid().toString(QUuid::WithoutBraces));
	IScriptController::SetMacro(scriptTemplate, IScriptController::Macro::Author, RemoveIllegalCharacters(book.author));
	IScriptController::SetMacro(scriptTemplate, IScriptController::Macro::Series, RemoveIllegalCharacters(book.series));
	IScriptController::SetMacro(scriptTemplate, IScriptController::Macro::SeqNumber, book.seqNumber > 0 ? QString::number(book.seqNumber) : QString {});
	IScriptController::SetMacro(scriptTemplate, IScriptController::Macro::FileSize, QString::number(book.size));
}
