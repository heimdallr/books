#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "interface/logic/ILogicFactory.h"

namespace Hypodermic {
class Container;
}

namespace HomeCompa::Flibrary {

class LogicFactory final : virtual public ILogicFactory
{
	NON_COPY_MOVABLE(LogicFactory)

public:
	explicit LogicFactory(Hypodermic::Container & container);
	~LogicFactory() override;

private: // ILogicFactory
	[[nodiscard]] std::shared_ptr<ITreeViewController> GetTreeViewController(ItemType type) const override;
	[[nodiscard]] std::shared_ptr<ArchiveParser> CreateArchiveParser() const override;
	[[nodiscard]] std::unique_ptr<Util::IExecutor> GetExecutor(Util::ExecutorInitializer initializer) const override;
	[[nodiscard]] std::shared_ptr<GroupController> CreateGroupController() const override;
	[[nodiscard]] std::shared_ptr<SearchController> CreateSearchController() const override;
	[[nodiscard]] std::shared_ptr<BooksContextMenuProvider> CreateBooksContextMenuProvider() const override;
	[[nodiscard]] std::shared_ptr<ReaderController> CreateReaderController() const override;
	[[nodiscard]] std::shared_ptr<IUserDataController> CreateUserDataController() const override;
	[[nodiscard]] std::shared_ptr<BooksExtractor> CreateBooksExtractor() const override;
	[[nodiscard]] std::shared_ptr<InpxCollectionExtractor> CreateInpxCollectionExtractor() const override;
	[[nodiscard]] std::shared_ptr<IUpdateChecker> CreateUpdateChecker() const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
