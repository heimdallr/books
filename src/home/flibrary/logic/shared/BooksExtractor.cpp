#include "BooksExtractor.h"

#include <filesystem>

#include <QTemporaryDir>
#include <QTimer>

#include "Util/IExecutor.h"

#include <plog/Log.h>

#include "interface/logic/ICollectionController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IProgressController.h"
#include "interface/logic/IScriptController.h"

#include "ImageRestore.h"
#include "zip.h"
#include "data/DataItem.h"
#include "data/DataProvider.h"

#include "inpx/src/util/constant.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

class IPathChecker  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IPathChecker() = default;
	virtual void Check(std::filesystem::path & path) = 0;
};

void Write(QIODevice & stream, const BookInfo & book, size_t & n)
{
	auto seqNumber = book.book->GetRawData(BookItem::Column::SeqNumber);
	if (seqNumber.toInt() <= 0)
		seqNumber = "0";

	const QFileInfo fileInfo(book.book->GetRawData(BookItem::Column::FileName));

	QStringList authors;
	for (const auto & author : book.authors)
	{
		const QStringList authorNames = QStringList()
			<< author->GetRawData(AuthorItem::Column::LastName)
			<< author->GetRawData(AuthorItem::Column::FirstName)
			<< author->GetRawData(AuthorItem::Column::MiddleName)
			;
		authors << authorNames.join(NAMES_SEPARATOR) << QString(LIST_SEPARATOR);
	}
	stream.write((authors.join("") + FIELDS_SEPARATOR).toUtf8());

	for (const auto & genre : book.genres)
		stream.write((genre->GetRawData(GenreItem::Column::Fb2Code) + LIST_SEPARATOR).toUtf8());
	stream.write(QString(FIELDS_SEPARATOR).toUtf8());

	stream.write((book.book->GetRawData(BookItem::Column::Title) + FIELDS_SEPARATOR).toUtf8());
	stream.write((book.book->GetRawData(BookItem::Column::Series) + FIELDS_SEPARATOR).toUtf8());
	stream.write((seqNumber + FIELDS_SEPARATOR).toUtf8());
	stream.write((fileInfo.completeBaseName() + FIELDS_SEPARATOR).toUtf8());
	stream.write((book.book->GetRawData(BookItem::Column::Size) + FIELDS_SEPARATOR).toUtf8());
	stream.write((book.book->GetId() + FIELDS_SEPARATOR).toUtf8());
	stream.write((QString(book.book->To<BookItem>()->removed ? "1" : "0") + FIELDS_SEPARATOR).toUtf8());
	stream.write((fileInfo.suffix() + FIELDS_SEPARATOR).toUtf8());
	stream.write((book.book->GetRawData(BookItem::Column::UpdateDate) + FIELDS_SEPARATOR).toUtf8());
	stream.write((QString::number(n++) + FIELDS_SEPARATOR).toUtf8());
	stream.write((book.book->GetRawData(BookItem::Column::Folder) + FIELDS_SEPARATOR).toUtf8());
	stream.write((book.book->GetRawData(BookItem::Column::Lang) + FIELDS_SEPARATOR + FIELDS_SEPARATOR).toUtf8());

	stream.write("\n");
}

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

std::pair<bool, std::filesystem::path> Write(QIODevice & input
	, const QString & dstFileName
	, const QString & folder
	, const ILogicFactory::ExtractedBook & book
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

	const auto bytes = RestoreImages(input, folder, book.file);
	progress.Increment(input.size());

	result.first = archive ? Archive(bytes, result.second, dstFileInfo.fileName()) : Write(bytes, result.second);

	return result;
}


std::filesystem::path Process(const std::filesystem::path & archiveFolder
	, const QString & dstFolder
	, const ILogicFactory::ExtractedBook & book
	, QString outputFileTemplate
	, IProgressController::IProgressItem & progress
	, IPathChecker & pathChecker
	, const bool asArchives
)
{
	if (progress.IsStopped())
		return {};

	IScriptController::SetMacro(outputFileTemplate, IScriptController::Macro::UserDestinationFolder, dstFolder);
	ILogicFactory::FillScriptTemplate(outputFileTemplate, book);

	const auto folder = QDir::fromNativeSeparators(QString::fromStdWString(archiveFolder / book.folder.toStdWString()));
	const Zip zip(folder);
	auto [ok, path] = Write(zip.Read(book.file), outputFileTemplate, folder, book, progress, pathChecker, asArchives);
	if (!ok && exists(path))
		remove(path);

	return ok ? path : std::filesystem::path{};
}

void Process(const std::filesystem::path & archiveFolder
	, const QString & dstFolder
	, const ILogicFactory::ExtractedBook & book
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
		ILogicFactory::FillScriptTemplate(command.args, book);

		if (false
			|| progress.IsStopped()
			|| !scriptController.Execute(command)
			)
			return;
	}
}

void Process(const std::filesystem::path & archiveFolder
	, const BookInfo & book
	, IProgressController::IProgressItem & progress
	, Zip & zipOutput
	, QIODevice & inpx
	, size_t & n
)
{
	const auto fileName = book.book->GetRawData(BookItem::Column::FileName);
	const auto folder = QString::fromStdWString(archiveFolder / book.book->GetRawData(BookItem::Column::Folder).toStdWString());
	auto & output = zipOutput.Write(fileName);
	const Zip zipInput(folder);
	auto & input = zipInput.Read(fileName);
	const auto bytes = RestoreImages(input, folder, fileName);
	output.write(bytes);
	Write(inpx, book, n);
	progress.Increment(input.size());
}

struct BookWrapper
{
	ILogicFactory::ExtractedBook extractedBook {};
	BookInfo bookInfo {};

	explicit BookWrapper(ILogicFactory::ExtractedBook && extractedBook)
		: extractedBook(std::move(extractedBook))
	{
	}

	explicit BookWrapper(BookInfo && bookInfo)
		: bookInfo(std::move(bookInfo))
	{
	}

	int64_t GetSize() const noexcept
	{
		return bookInfo.book ? bookInfo.book->GetData(BookItem::Column::Size).toLongLong() : extractedBook.size;
	}

	const QString & GetFile() const noexcept
	{
		return bookInfo.book ? bookInfo.book->GetData(BookItem::Column::FileName) : extractedBook.file;
	}
};

using BookWrappers = std::vector<BookWrapper>;

using ProcessFunctor = std::function<void(const std::filesystem::path & archiveFolder
	, const QString & dstFolder
	, const BookWrapper & book
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

	void Extract(QString dstFolder, BookWrappers && books, Callback callback, ProcessFunctor processFunctor, const int maxThreadCount = std::numeric_limits<int>::max())
	{
		assert(!m_callback);
		m_hasError = false;
		m_callback = std::move(callback);
		m_taskCount = std::size(books);
		m_processFunctor = std::move(processFunctor);
		m_logicFactory->GetExecutor({ std::min(static_cast<int>(m_taskCount), maxThreadCount)}).swap(m_executor);
		m_dstFolder = std::move(dstFolder);
		m_archiveFolder = m_collectionController->GetActiveCollection()->folder.toStdWString();

		std::for_each(std::make_move_iterator(std::begin(books)), std::make_move_iterator(std::end(books)), [&] (BookWrapper && book)
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
	Util::IExecutor::Task CreateTask(BookWrapper && book)
	{
		std::shared_ptr progressItem = m_progressController->Add(book.GetSize());
		auto taskName = book.GetFile().toStdString();
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

template<typename T>
BookWrappers CreateBookWrappers(T && books)
{
	BookWrappers bookWrappers;
	std::transform(std::make_move_iterator(books.begin()), std::make_move_iterator(books.end()), std::back_inserter(bookWrappers), [] (typename T::value_type && book)
	{
		return BookWrapper(std::move(book));
	});
	return bookWrappers;
}

void BooksExtractor::ExtractAsArchives(QString folder, const QString &/*parameter*/, ILogicFactory::ExtractedBooks && books, QString outputFileNameTemplate, Callback callback)
{
	BookWrappers bookWrappers = CreateBookWrappers(std::move(books));
	m_impl->Extract(std::move(folder), std::move(bookWrappers), std::move(callback)
		, [outputFileNameTemplate = std::move(outputFileNameTemplate)] (const std::filesystem::path & archiveFolder
			, const QString & dstFolder
			, const BookWrapper & book
			, IProgressController::IProgressItem & progress
			, IPathChecker & pathChecker
			)
	{
		Process(archiveFolder, dstFolder, book.extractedBook, outputFileNameTemplate, progress, pathChecker, true);
	});
}

void BooksExtractor::ExtractAsIs(QString folder, const QString &/*parameter*/, ILogicFactory::ExtractedBooks && books, QString outputFileNameTemplate, Callback callback)
{
	BookWrappers bookWrappers = CreateBookWrappers(std::move(books));
	m_impl->Extract(std::move(folder), std::move(bookWrappers), std::move(callback)
		, [outputFileNameTemplate = std::move(outputFileNameTemplate)] (const std::filesystem::path & archiveFolder
			, const QString & dstFolder
			, const BookWrapper & book
			, IProgressController::IProgressItem & progress
			, IPathChecker & pathChecker
			)
	{
		Process(archiveFolder, dstFolder, book.extractedBook, outputFileNameTemplate, progress, pathChecker, false);
	});
}

void BooksExtractor::ExtractAsScript(QString folder, const QString &parameter, ILogicFactory::ExtractedBooks && books, QString outputFileNameTemplate, Callback callback)
{
	BookWrappers bookWrappers = CreateBookWrappers(std::move(books));
	auto scriptController = m_impl->GetScriptController();
	auto commands = scriptController->GetCommands(parameter);
	m_impl->Extract(std::move(folder), std::move(bookWrappers), std::move(callback)
		, [scriptController = std::move(scriptController)
			, commands = std::move(commands)
			, tempDir = std::make_shared<QTemporaryDir>()
			, outputFileNameTemplate = std::move(outputFileNameTemplate)
		]
		(const std::filesystem::path & archiveFolder
			, const QString & dstFolder
			, const BookWrapper & book
			, IProgressController::IProgressItem & progress
			, IPathChecker & pathChecker
			)
	{
		Process(archiveFolder, dstFolder, book.extractedBook, outputFileNameTemplate, progress, pathChecker, *scriptController, commands, *tempDir);
	});
}

void BooksExtractor::ExtractAsInpxCollection(QString folder, const std::vector<QString> & idList, const DataProvider & dataProvider, Callback callback)
{
	std::vector<BookInfo> bookInfo;
	std::ranges::transform(idList, std::back_inserter(bookInfo), [&] (const auto & id) { return dataProvider.GetBookInfo(id.toLongLong()); });
	BookWrappers bookWrappers = CreateBookWrappers(std::move(bookInfo));
	const auto archiveName = QDateTime::currentDateTime().toString("yyyyMMddhhmmss");
	auto zip = std::make_shared<Zip>(QString("%1/%2.zip").arg(folder, archiveName), Zip::Format::Zip);
	auto inpx = std::make_shared<Zip>(QString("%1/collection.inpx").arg(folder, archiveName), Zip::Format::Zip);
	auto & inpxStream = inpx->Write(QString("%1.inp").arg(archiveName));
	m_impl->Extract(std::move(folder), std::move(bookWrappers), std::move(callback), [zip = std::move(zip), inpx = std::move(inpx), &inpxStream, n = size_t{0}]
		(const std::filesystem::path & archiveFolder
			, const QString & /*dstFolder*/
			, const BookWrapper & book
			, IProgressController::IProgressItem & progress
			, IPathChecker &
			) mutable
	{
		Process(archiveFolder, book.bookInfo, progress, *zip, inpxStream, n);
	}, 1);
}
