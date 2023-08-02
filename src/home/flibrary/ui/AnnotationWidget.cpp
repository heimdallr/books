#include "ui_AnnotationWidget.h"
#include "AnnotationWidget.h"

#include <plog/Log.h>

#include "util/ISettings.h"

using namespace HomeCompa::Flibrary;

namespace {
constexpr auto SPLITTER_KEY = "ui/Annotation/Splitter";
}

class AnnotationWidget::Impl
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(AnnotationWidget & self
		, std::shared_ptr<ISettings> settings
	)
		: m_self(self)
		, m_settings(std::move(settings))
	{
		m_ui.setupUi(&m_self);
		m_ui.picture->setVisible(false);

		if (const auto value = m_settings->Get(SPLITTER_KEY); value.isValid())
			m_ui.splitter->restoreState(value.toByteArray());
	}

	~Impl()
	{
		m_settings->Set(SPLITTER_KEY, m_ui.splitter->saveState());
	}

private:
	AnnotationWidget & m_self;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	Ui::AnnotationWidget m_ui {};
};

AnnotationWidget::AnnotationWidget(std::shared_ptr<ISettings> settings
	, QWidget * parent
)
	: QWidget(parent)
	, m_impl(*this, std::move(settings))
{
	PLOGD << "AnnotationWidget created";
}

AnnotationWidget::~AnnotationWidget()
{
	PLOGD << "AnnotationWidget destroyed";
}
