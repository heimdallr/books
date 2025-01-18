#include "ui_ComboBoxTextDialog.h"
#include "ComboBoxTextDialog.h"

#include <QPushButton>

#include "GuiUtil/GeometryRestorable.h"
#include "interface/ui/IUiFactory.h"

using namespace HomeCompa::Flibrary;

struct ComboBoxTextDialog::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	Ui::ComboBoxTextDialog ui {};

	Impl(QWidget & self, std::shared_ptr<ISettings> settings)
		: GeometryRestorable(*this, std::move(settings), "ComboBoxTextDialog")
		, GeometryRestorableObserver(self)
		, m_self(self)
	{
		Init();
	}

private: // GeometryRestorableObserver
	void OnFontChanged(const QFont &) override
	{
		m_self.adjustSize();
		const auto height = m_self.sizeHint().height();
		if (height < 0)
			return;

		m_self.setMinimumHeight(height);
		m_self.setMaximumHeight(height);
	}

private:
	QWidget & m_self;
};

ComboBoxTextDialog::ComboBoxTextDialog(const std::shared_ptr<const IUiFactory> & uiFactory
	, std::shared_ptr<ISettings> settings
	, QWidget *parent
)
	: QDialog(parent ? parent : uiFactory->GetParentWidget())
	, m_impl(*this, std::move(settings))
{
	auto & ui = m_impl->ui;
	ui.setupUi(this);
	setWindowTitle(uiFactory->GetTitle());

	auto setOkButtonEnabled = [this] (const QString & text) { m_impl->ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!text.isEmpty()); };
	setOkButtonEnabled(ui.lineEdit->text());
	connect(ui.lineEdit, &QLineEdit::textChanged, std::move(setOkButtonEnabled));
}

ComboBoxTextDialog::~ComboBoxTextDialog() = default;

QDialog & ComboBoxTextDialog::GetDialog()
{
	return *this;
}

QComboBox & ComboBoxTextDialog::GetComboBox()
{
	assert(m_impl->ui.comboBox);
	return *m_impl->ui.comboBox;
}

QLineEdit & ComboBoxTextDialog::GetLineEdit()
{
	assert(m_impl->ui.lineEdit);
	return *m_impl->ui.lineEdit;
}
