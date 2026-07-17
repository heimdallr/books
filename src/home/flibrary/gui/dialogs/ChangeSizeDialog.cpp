#include "ui_ChangeSizeDialog.h"

#include "ChangeSizeDialog.h"

using namespace HomeCompa::Flibrary;

class ChangeSizeDialog::Impl
{
public:
	Impl(QWidget* self, const int current, const int minimum, const int maximum, IUiFactory::IChangeSizeWidgetObserver* observer)
	{
		m_ui.setupUi(self);
		m_ui.slider->setMinimum(minimum);
		m_ui.slider->setMaximum(maximum);
		m_ui.spinBox->setMinimum(minimum);
		m_ui.spinBox->setMaximum(maximum);

		m_ui.slider->setValue(current);

		connect(m_ui.slider, &QSlider::valueChanged, self, [observer](const int value) {
			observer->OnSizeChanged(value);
		});
	}

private:
	Ui::ChangeSizeDialog m_ui {};
};

ChangeSizeDialog::ChangeSizeDialog(const int current, const int minimum, const int maximum, IUiFactory::IChangeSizeWidgetObserver* observer, QWidget* parent)
	: QWidget(parent)
	, m_impl(this, current, minimum, maximum, observer)
{
}

ChangeSizeDialog::~ChangeSizeDialog() = default;
