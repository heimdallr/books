#include "ui_ProgressBar.h"
#include "ProgressBar.h"

#include <plog/Log.h>

#include "interface/logic/IProgressController.h"

using namespace HomeCompa::Flibrary;

class ProgressBar::Impl final
	: IProgressController::IObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	explicit Impl(ProgressBar & self
		, std::shared_ptr<IProgressController> progressController
	)
		: m_self(self)
		, m_progressController(std::move(progressController))
	{
		m_ui.setupUi(&m_self);
		m_progressController->RegisterObserver(this);

		OnStartedChanged();
	}

	~Impl() override
	{
		m_progressController->UnregisterObserver(this);
	}

private: // IProgressController::IObserver
	void OnStartedChanged() override
	{
		m_self.setVisible(m_progressController->IsStarted());
	}

	void OnValueChanged() override
	{
		m_ui.bar->setValue(static_cast<int>(std::lround(m_ui.bar->minimum() + m_progressController->GetValue() * (m_ui.bar->maximum() - m_ui.bar->minimum()))));
	}

private:
	ProgressBar & m_self;
	PropagateConstPtr<IProgressController, std::shared_ptr> m_progressController;
	Ui::ProgressBar m_ui {};
};

ProgressBar::ProgressBar(std::shared_ptr<IProgressController> progressController
	, QWidget * parent
)
	: QWidget(parent)
	, m_impl(*this, std::move(progressController))
{
	PLOGD << "ProgressBar created";
}

ProgressBar::~ProgressBar()
{
	PLOGD << "ProgressBar destroyed";
}
