#include "BooksExtractor.h"

#include <map>
#include <ranges>
#include <thread>

#include "Util/IExecutor.h"

#include <plog/Log.h>

#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IProgressController.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

void Process(const BooksExtractor::Book & book, IProgressController::IProgressItem & progressItem)
{
	for (int i = 0; !progressItem.IsStopped() && i < 20; i++, std::this_thread::sleep_for(std::chrono::milliseconds(100)))
		progressItem.Increment(book.size / 20);
}

}

class BooksExtractor::Impl
{
public:
	Impl(std::shared_ptr<ICollectionController> collectionController
		, std::shared_ptr<IProgressController> progressController
		, std::shared_ptr<ILogicFactory> logicFactory
	)
		: m_collectionController(std::move(collectionController))
		, m_progressController(std::move(progressController))
		, m_logicFactory(std::move(logicFactory))
	{
	}

	void ExtractAsArchives(const QString & /*folder*/, Books && /*books*/, Callback /*callback*/)
	{
	}

	void ExtractAsIs(const QString & /*folder*/, Books && books, Callback callback)
	{
		assert(!m_callback);
		m_callback = std::move(callback);

		std::transform(std::make_move_iterator(std::begin(books)), std::make_move_iterator(std::end(books)), std::back_inserter(m_tasks), [&] (Book && book)
		{
			return CreateTask(std::move(book));
		});
		PLOGD << std::size(m_tasks) << " tasks created";

		const auto cpuCount = static_cast<int>(std::thread::hardware_concurrency());
		const auto executorCount = std::min(cpuCount, static_cast<int>(std::size(m_tasks)));
		std::generate_n(std::back_inserter(m_executors), executorCount, [&]() mutable
		{
			return m_logicFactory->GetExecutor();
		});
		PLOGD << std::size(m_executors) << " executors created";

		std::ranges::for_each(std::ranges::iota_view { 0, executorCount }, [&] (const int i)
		{
			Run(i);
		});
	}

private:
	void Run(const int i)
	{
		if (m_tasks.empty())
		{
			m_executors[i].reset();

			if (m_running.empty())
				return m_callback();

			return;
		}

		auto task = std::move(m_tasks.back());
		m_tasks.pop_back();

		m_running.emplace((*m_executors[i])(std::move(task)), i);
	}

	Util::IExecutor::Task CreateTask(Book && book)
	{
		std::shared_ptr progressItem = m_progressController->Add(book.size);
		auto taskName = book.file.toStdString();
		return Util::IExecutor::Task
		{
			std::move(taskName), [&, book = std::move(book), progressItem = std::move(progressItem)] ()
			{
				Process(book, *progressItem);
				return [&] (const size_t id)
				{
					const auto it = m_running.find(id);
					assert(it != m_running.end());
					const auto n = it->second;
					m_running.erase(it);
					Run(n);
				};
			}
		};
	}

private:
	PropagateConstPtr<ICollectionController, std::shared_ptr> m_collectionController;
	PropagateConstPtr<IProgressController, std::shared_ptr> m_progressController;
	PropagateConstPtr<ILogicFactory, std::shared_ptr> m_logicFactory;
	Callback m_callback;
	std::vector<Util::IExecutor::Task> m_tasks;
	std::vector<std::unique_ptr<Util::IExecutor>> m_executors;
	std::map<size_t, int> m_running;
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
	m_impl->ExtractAsArchives(folder, std::move(books), std::move(callback));
}

void BooksExtractor::ExtractAsIs(const QString & folder, Books && books, Callback callback)
{
	m_impl->ExtractAsIs(folder, std::move(books), std::move(callback));
}
