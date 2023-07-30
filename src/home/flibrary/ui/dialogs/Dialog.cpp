#include "Dialog.h"

#include <plog/Log.h>

#include "interface/constants/Localization.h"

#include "ParentWidgetProvider.h"

using namespace HomeCompa::Flibrary;

Dialog::Dialog(std::shared_ptr<ParentWidgetProvider> parentProvider)
	: m_parentProvider(std::move(parentProvider))
{
	PLOGD << "Dialog created";
}

Dialog::~Dialog()
{
	PLOGD << "Dialog destroyed";
}

QuestionDialog::QuestionDialog(std::shared_ptr<ParentWidgetProvider> parentProvider)
	: Dialog(std::move(parentProvider))
{
}

QMessageBox::StandardButton QuestionDialog::Show(const QString & text, const QMessageBox::StandardButtons buttons, const QMessageBox::StandardButton defaultButton) const
{
	return QMessageBox::question(m_parentProvider->GetWidget(), Loc::Question(), text, buttons, defaultButton);
}


WarningDialog::WarningDialog(std::shared_ptr<ParentWidgetProvider> parentProvider)
	: Dialog(std::move(parentProvider))
{
}

QMessageBox::StandardButton WarningDialog::Show(const QString & text, const QMessageBox::StandardButtons buttons, const QMessageBox::StandardButton defaultButton) const
{
	return QMessageBox::warning(m_parentProvider->GetWidget(), Loc::Warning(), text, buttons, defaultButton);
}
