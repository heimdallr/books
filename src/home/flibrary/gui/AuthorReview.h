#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IModelProvider.h"
#include "interface/logic/IReaderController.h"
#include "interface/ui/IUiFactory.h"

#include "util/ISettings.h"

#include "ScrollBarController.h"
#include "StackedPage.h"

namespace HomeCompa::Flibrary
{

class AuthorReview final : public StackedPage
{
	NON_COPY_MOVABLE(AuthorReview)

public:
	AuthorReview(
		const std::shared_ptr<const IUiFactory>&     uiFactory,
		const std::shared_ptr<const IModelProvider>& modelProvider,
		std::shared_ptr<const IReaderController>     readerController,
		std::shared_ptr<ISettings>                   settings,
		std::shared_ptr<ScrollBarController>         scrollBarController,
		QWidget*                                     parent = nullptr
	);
	~AuthorReview() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
