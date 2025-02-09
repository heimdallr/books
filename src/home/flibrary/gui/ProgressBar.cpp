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
		connect(m_ui.button, &QAbstractButton::clicked, &m_self, [&]
		{
			m_progressController->Stop();
		});
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
		if (!m_progressController->IsStarted())
			m_ui.bar->setValue(0);
	}

	void OnValueChanged() override
	{
		const auto logicValue = m_progressController->GetValue();
		const auto uiValue = static_cast<int>(m_ui.bar->minimum() + logicValue * (m_ui.bar->maximum() - m_ui.bar->minimum()) + std::numeric_limits<double>::epsilon());
		if (const auto loggedValue = uiValue / 10; m_loggedValue != loggedValue)
			PLOGV << (m_loggedValue = loggedValue) * 10 << "%";

		m_ui.bar->setValue(uiValue);
	}

	void OnStop() override
	{
		PLOGD << "Cancel progress requested";
	}

private:
	ProgressBar & m_self;
	PropagateConstPtr<IProgressController, std::shared_ptr> m_progressController;
	Ui::ProgressBar m_ui {};
	int m_loggedValue { 0 };
};

ProgressBar::ProgressBar(std::shared_ptr<IBooksExtractorProgressController> progressController
	, QWidget * parent
)
	: QWidget(parent)
	, m_impl(*this, std::move(progressController))
{
	PLOGV << "ProgressBar created";
}

ProgressBar::~ProgressBar()
{
	PLOGV << "ProgressBar destroyed";
}
