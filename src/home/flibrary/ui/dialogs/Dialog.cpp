#include "Dialog.h"

#include <QInputDialog>

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

#define STANDARD_DIALOG_ITEM(NAME) \
NAME##Dialog::NAME##Dialog(std::shared_ptr<ParentWidgetProvider> parentProvider) \
	: Dialog(std::move(parentProvider)) {}
STANDARD_DIALOG_ITEMS_X_MACRO
#undef	STANDARD_DIALOG_ITEM

#define NO_GET_TEXT(NAME) QString NAME##Dialog::GetText(const QString & /*title*/, const QString & /*label*/, const QString & /*text*/, QLineEdit::EchoMode /*mode*/) const { throw std::runtime_error("not implemented"); }
		NO_GET_TEXT(About)
		NO_GET_TEXT(Error)
		NO_GET_TEXT(Info)
		NO_GET_TEXT(Question)
		NO_GET_TEXT(Warning)
#undef	NO_GET_TEXT

#define NO_SHOW(NAME) QMessageBox::StandardButton NAME##Dialog::Show(const QString & /*text*/ = {}, QMessageBox::StandardButtons /*buttons*/ = QMessageBox::Ok, QMessageBox::StandardButton /*defaultButton*/ = QMessageBox::NoButton) const { throw std::runtime_error("not implemented"); }
		NO_SHOW(InputText)
#undef	NO_SHOW

QMessageBox::StandardButton AboutDialog::Show(const QString & /*text*/, QMessageBox::StandardButtons /*buttons*/, QMessageBox::StandardButton /*defaultButton*/) const
{
	QMessageBox messageBox(m_parentProvider->GetWidget());
	messageBox.setWindowTitle(Loc::Tr(CONTEXT, ABOUT_TITLE));
	messageBox.setTextFormat(Qt::RichText);
	messageBox.setText(Loc::Tr(CONTEXT, ABOUT_TEXT).arg(GetApplicationVersion(), "https://github.com/heimdallr/books"));
	messageBox.exec();
	return QMessageBox::NoButton;
}

QMessageBox::StandardButton QuestionDialog::Show(const QString & text, const QMessageBox::StandardButtons buttons, const QMessageBox::StandardButton defaultButton) const
{
	return QMessageBox::question(m_parentProvider->GetWidget(), Loc::Question(), text, buttons, defaultButton);
}

QMessageBox::StandardButton WarningDialog::Show(const QString & text, const QMessageBox::StandardButtons buttons, const QMessageBox::StandardButton defaultButton) const
{
	return QMessageBox::warning(m_parentProvider->GetWidget(), Loc::Warning(), text, buttons, defaultButton);
}

QMessageBox::StandardButton InfoDialog::Show(const QString & text, const QMessageBox::StandardButtons buttons, const QMessageBox::StandardButton defaultButton) const
{
	return QMessageBox::information(m_parentProvider->GetWidget(), Loc::Information(), text, buttons, defaultButton);
}

QMessageBox::StandardButton ErrorDialog::Show(const QString & text, const QMessageBox::StandardButtons buttons, const QMessageBox::StandardButton defaultButton) const
{
	return QMessageBox::critical(m_parentProvider->GetWidget(), Loc::Error(), text, buttons, defaultButton);
}

QString InputTextDialog::GetText(const QString & title, const QString & label, const QString & text, const QLineEdit::EchoMode mode) const
{
	return QInputDialog::getText(m_parentProvider->GetWidget(), title, label, mode, text);
}
