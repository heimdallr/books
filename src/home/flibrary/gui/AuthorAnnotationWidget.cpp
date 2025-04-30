#include "ui_AuthorAnnotationWidget.h"

#include "AuthorAnnotationWidget.h"

#include <QTimer>

#include "interface/logic/ITreeViewController.h"

using namespace HomeCompa::Flibrary;

class AuthorAnnotationWidget::Impl final : IAuthorAnnotationController::IObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(QFrame* self, std::shared_ptr<IAuthorAnnotationController> annotationController)
		: m_self { self }
		, m_annotationController { std::move(annotationController) }
	{
		m_ui.setupUi(m_self);

		m_annotationController->RegisterObserver(this);
	}

	~Impl() override
	{
		m_annotationController->UnregisterObserver(this);
	}

	void Show(const bool value)
	{
		m_show = value;
		SetVisible();
	}

private: // IAuthorAnnotationController::IObserver
	void OnReadyChanged() override
	{
		SetVisible();
	}

	void OnAuthorChanged(const QString& text, const std::vector<QByteArray>& /*images*/) override
	{
		m_ui.info->setText(text);
	}

private:
	void SetVisible() const
	{
		QTimer::singleShot(0, [this] { m_self->parentWidget()->setVisible(m_show && m_annotationController->IsReady()); });
	}

private:
	QFrame* m_self;
	PropagateConstPtr<IAuthorAnnotationController, std::shared_ptr> m_annotationController;
	bool m_show { true };
	Ui::AuthorAnnotationWidget m_ui {};
};

AuthorAnnotationWidget::AuthorAnnotationWidget(std::shared_ptr<IAuthorAnnotationController> annotationController, QWidget* parent)
	: QFrame(parent)
	, m_impl(this, std::move(annotationController))
{
}

AuthorAnnotationWidget::~AuthorAnnotationWidget() = default;

void AuthorAnnotationWidget::Show(const bool value)
{
	m_impl->Show(value);
}
