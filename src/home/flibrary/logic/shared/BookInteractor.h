#pragma once

#include "interface/logic/IBookExtractor.h"
#include "interface/logic/IBookInteractor.h"
#include "interface/logic/IReaderController.h"

#include "BooksContextMenuProvider.h"

namespace HomeCompa::Flibrary
{

class BookInteractor final : public IBookInteractor
{
	NON_COPY_MOVABLE(BookInteractor)

public:
	BookInteractor(
		const std::shared_ptr<const ILogicFactory>& logicFactory,
		std::shared_ptr<const IUiFactory>           uiFactory,
		std::shared_ptr<const ISettings>            settings,
		std::shared_ptr<const IBookExtractor>       bookExtractor,
		std::shared_ptr<const IReaderController>    readerController

	);
	~BookInteractor() override;

private:
	void OnLinkActivated(long long bookId) const override;
	void OnDoubleClicked(long long bookId) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
