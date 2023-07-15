#include "Dialog.h"
#include "ParentWidgetProvider.h"

using namespace HomeCompa::Flibrary;

Dialog::Dialog(std::shared_ptr<class ParentWidgetProvider> parentProvider)
	: m_parentProvider(std::move(parentProvider))
{
}

Dialog::~Dialog() = default;

WarningDialog::WarningDialog(std::shared_ptr<ParentWidgetProvider> parentProvider)
	: Dialog(std::move(parentProvider))
{
}

QMessageBox::StandardButton WarningDialog::Show(const QString & title, const QString & text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) const
{
	return QMessageBox::warning(m_parentProvider->GetWidget(), title, text, buttons, defaultButton);
}
