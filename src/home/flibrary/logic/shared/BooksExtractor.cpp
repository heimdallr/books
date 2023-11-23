#include "BooksExtractor.h"

#include <filesystem>

#include <QFile>
#include <QRegularExpression>
#include <QTimer>

#include "Util/IExecutor.h"

#include <plog/Log.h>

#include "interface/logic/ICollectionController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IProgressController.h"

#include "util/zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

class IPathChecker  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IPathChecker() = default;
	virtual void Check(std::filesystem::path & path) = 0;
};

bool Copy(QIODevice & input, QIODevice & output, IProgressController::IProgressItem & progress)
{
	if (progress.IsStopped())
		return false;

	static constexpr auto bufferSize = 16 * 1024ll;
	const std::unique_ptr<char[]> buffer(new char[bufferSize]);
	while (const auto size = input.read(buffer.get(), bufferSize))
	{
		if (output.write(buffer.get(), size) != size)
			return false;

		progress.Increment(size);
		if (progress.IsStopped())
			return false;
	}

	return true;
}

bool Write(QIODevice & input, const std::filesystem::path & path, IProgressController::IProgressItem & progress)
{
	QFile output(QString::fromStdWString(path));
	output.open(QIODevice::WriteOnly);
	return Copy(input, output, progress);
}

bool Archive(QIODevice & input, const std::filesystem::path & path, const QString & fileName, IProgressController::IProgressItem & progress)
{
	Util::Zip zip(QString::fromStdWString(path), Util::Zip::Format::Zip);
	auto & stream = zip.Write(fileName);
	return Copy(input, stream, progress);
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

std::pair<bool, std::filesystem::path> Write(QIODevice & input, std::filesystem::path dstPath, const BooksExtractor::Book & book, IProgressController::IProgressItem & progress, IPathChecker & pathChecker, const bool archive)
{
	std::pair<bool, std::filesystem::path> result { false, std::filesystem::path{} };
	if (!(exists(dstPath) || create_directory(dstPath)))
		return result;

	dstPath /= RemoveIllegalCharacters(book.author).toStdWString();
	if (!(exists(dstPath) || create_directory(dstPath)))
		return result;

	if (!book.series.isEmpty())
	{
		dstPath /= RemoveIllegalCharacters(book.series).toStdWString();
		if (!(exists(dstPath) || create_directory(dstPath)))
			return result;
	}

	const auto ext = std::filesystem::path(book.file.toStdWString()).extension();
	const auto fileName = (book.seqNumber > 0 ? QString::number(book.seqNumber) + "-" : "") + book.title;

	result.second = (dstPath / (RemoveIllegalCharacters(fileName).toStdWString() + ext.wstring())).make_preferred();
	if (archive)
		result.second.replace_extension(".zip");

	pathChecker.Check(result.second);

	if (exists(result.second))
		if(!remove(result.second))
			return result;

	result.first = archive ? Archive(input, result.second, fileName + QString::fromStdWString(ext), progress) : Write(input, result.second, progress);

	return result;
}


void Process(const std::filesystem::path & archiveFolder, const std::filesystem::path & dstFolder, const BooksExtractor::Book & book, IProgressController::IProgressItem & progress, IPathChecker & pathChecker, const bool asArchives)
{
	if (progress.IsStopped())
		return;

	const Util::Zip zip(QString::fromStdWString(archiveFolder / book.folder.toStdWString()));
	if (const auto [ok, path] = Write(zip.Read(book.file), dstFolder, book, progress, pathChecker, asArchives); !ok && exists(path))
		remove(path);
}

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
	)
		: m_collectionController(std::move(collectionController))
		, m_progressController(std::move(progressController))
		, m_logicFactory(std::move(logicFactory))
	{
		m_progressController->RegisterObserver(this);
	}

	~Impl() override
	{
		m_progressController->UnregisterObserver(this);
	}

	void Extract(const QString & dstFolder, Books && books, Callback callback, const bool asArchives)
	{
		assert(!m_callback);
		m_callback = std::move(callback);
		m_taskCount = std::size(books);
		m_asArchives = asArchives;
		m_logicFactory->GetExecutor({ static_cast<int>(m_taskCount)}).swap(m_executor);
		m_dstFolder = dstFolder.toStdWString();
		m_archiveFolder = m_collectionController->GetActiveCollection()->folder.toStdWString();

		std::for_each(std::make_move_iterator(std::begin(books)), std::make_move_iterator(std::end(books)), [&] (Book && book)
		{
			(*m_executor)(CreateTask(std::move(book)));
		});
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
			m_callback();
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
				Process(m_archiveFolder, m_dstFolder, book, *progressItem, *this, m_asArchives);
				return [&] (const size_t)
				{
					assert(m_taskCount > 0);
					if (--m_taskCount == 0)
						m_callback();
				};
			}
		};
	}

private:
	PropagateConstPtr<ICollectionController, std::shared_ptr> m_collectionController;
	PropagateConstPtr<IProgressController, std::shared_ptr> m_progressController;
	PropagateConstPtr<ILogicFactory, std::shared_ptr> m_logicFactory;
	Callback m_callback;
	size_t m_taskCount { 0 };
	bool m_asArchives { false };
	std::unique_ptr<Util::IExecutor> m_executor;
	std::filesystem::path m_dstFolder;
	std::filesystem::path m_archiveFolder;
	std::mutex m_usedPathGuard;
	std::unordered_set<QString> m_usedPath;
};

BooksExtractor::BooksExtractor(std::shared_ptr<ICollectionController> collectionController
	, std::shared_ptr<IProgressController> progressController
	, std::shared_ptr<ILogicFactory> logicFactory
)
	: m_impl(std::move(collectionController), std::move(progressController), std::move(logicFactory))
{
	PLOGD << "BooksExtractor created";
}

BooksExtractor::~BooksExtractor()
{
	PLOGD << "BooksExtractor destroyed";
}

void BooksExtractor::ExtractAsArchives(const QString & folder, Books && books, Callback callback)
{
	m_impl->Extract(folder, std::move(books), std::move(callback), true);
}

void BooksExtractor::ExtractAsIs(const QString & folder, Books && books, Callback callback)
{
	m_impl->Extract(folder, std::move(books), std::move(callback), false);
}
