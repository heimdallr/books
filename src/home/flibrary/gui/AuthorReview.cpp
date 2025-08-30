#include "ui_AuthorReview.h"

#include "AuthorReview.h"

#include "interface/logic/AuthorReviewModelRole.h"

#include "gutil/GeometryRestorable.h"

using namespace HomeCompa::Flibrary;

class AuthorReview::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(StackedPage& self, const IUiFactory& uiFactory, const IModelProvider& modelProvider, std::shared_ptr<ISettings> settings, std::shared_ptr<ScrollBarController> scrollBarController)
		: GeometryRestorable(*this, std::move(settings), "AuthorReview")
		, GeometryRestorableObserver(self)
		, m_model { modelProvider.CreateAuthorReviewModel() }
		, m_scrollBarController { std::move(scrollBarController) }
	{
		m_ui.setupUi(&self);
		connect(m_ui.btnClose, &QAbstractButton::clicked, [&] { self.StateChanged(State::Finished); });
		self.addAction(m_ui.actionClose);

		m_ui.view->setModel(m_model.get());
		m_model->setData({}, uiFactory.GetAuthorId(), AuthorReviewModelRole::AuthorId);

		m_scrollBarController->SetScrollArea(m_ui.view);
		m_ui.view->viewport()->installEventFilter(m_scrollBarController.get());

		LoadGeometry();
	}

	~Impl() override
	{
		SaveGeometry();
	}

private:
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_model;
	PropagateConstPtr<ScrollBarController, std::shared_ptr> m_scrollBarController;
	Ui::AuthorReview m_ui;
};

AuthorReview::AuthorReview(const std::shared_ptr<const IUiFactory>& uiFactory,
                           const std::shared_ptr<const IModelProvider>& modelProvider,
                           std::shared_ptr<ISettings> settings,
                           std::shared_ptr<ScrollBarController> scrollBarController,
                           QWidget* parent)
	: StackedPage(parent)
	, m_impl(*this, *uiFactory, *modelProvider, std::move(settings), std::move(scrollBarController))
{
}

AuthorReview::~AuthorReview() = default;
