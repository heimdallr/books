#include "Dialog.h"

#include <plog/Log.h>

#include "ParentWidgetProvider.h"

using namespace HomeCompa::Flibrary;

Dialog::Dialog(std::shared_ptr<class ParentWidgetProvider> parentProvider)
	: m_parentProvider(std::move(parentProvider))
{
	PLOGD << "Dialog created";
}

Dialog::~Dialog()
{
	PLOGD << "Dialog destroyed";
}

WarningDialog::WarningDialog(std::shared_ptr<ParentWidgetProvider> parentProvider)
	: Dialog(std::move(parentProvider))
{
}

QMessageBox::StandardButton WarningDialog::Show(const QString & title, const QString & text, const QMessageBox::StandardButtons buttons, const QMessageBox::StandardButton defaultButton) const
{
	return QMessageBox::warning(m_parentProvider->GetWidget(), title, text, buttons, defaultButton);
}
