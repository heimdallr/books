#include "BooksExtractor.h"

#include "Util/IExecutor.h"

#include <plog/Log.h>

#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IProgressController.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

void Process(const BooksExtractor::Book & /*book*/, IProgressController::IProgressItem & /*progressItem*/)
{
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
		m_taskCount = std::size(books);
		m_logicFactory->GetExecutor({ static_cast<int>(m_taskCount)}).swap(m_executor);

		std::for_each(std::make_move_iterator(std::begin(books)), std::make_move_iterator(std::end(books)), [&] (Book && book)
		{
			(*m_executor)(CreateTask(std::move(book)));
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
				Process(book, *progressItem);
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
	std::unique_ptr<Util::IExecutor> m_executor;
	size_t m_taskCount { 0 };
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
