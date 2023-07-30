#include "Dialog.h"

#include <plog/Log.h>

#include "interface/constants/Localization.h"

#include "ParentWidgetProvider.h"

#include "version/AppVersion.h"

using namespace HomeCompa::Flibrary;

namespace {
constexpr auto CONTEXT = "Dialog";
constexpr auto ABOUT_TITLE = QT_TRANSLATE_NOOP("Dialog", "About FLibrary");
constexpr auto ABOUT_TEXT = QT_TRANSLATE_NOOP("Dialog", "Another e-library book cataloger<p>Version: %1<p><a href='%2'>%2</a>");
}

Dialog::Dialog(std::shared_ptr<ParentWidgetProvider> parentProvider)
	: m_parentProvider(std::move(parentProvider))
{
	PLOGD << "Dialog created";
}

Dialog::~Dialog()
{
	PLOGD << "Dialog destroyed";
}

AboutDialog::AboutDialog(std::shared_ptr<ParentWidgetProvider> parentProvider)
	: Dialog(std::move(parentProvider))
{
}

QMessageBox::StandardButton AboutDialog::Show(const QString & /*text*/, QMessageBox::StandardButtons /*buttons*/, QMessageBox::StandardButton /*defaultButton*/) const
{
	QMessageBox messageBox(m_parentProvider->GetWidget());
	messageBox.setWindowTitle(Loc::Tr(CONTEXT, ABOUT_TITLE));
	messageBox.setTextFormat(Qt::RichText);
	messageBox.setText(Loc::Tr(CONTEXT, ABOUT_TEXT).arg(GetApplicationVersion(), "https://github.com/heimdallr/books"));
	messageBox.exec();
//	QMessageBox::about(m_parentProvider->GetWidget(), Loc::Tr(CONTEXT, ABOUT_TITLE), Loc::Tr(CONTEXT, ABOUT_TEXT).arg(GetApplicationVersion(), "https://github.com/heimdallr/books"));
	return QMessageBox::NoButton;
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
