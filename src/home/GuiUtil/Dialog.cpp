#include "Dialog.h"

#include <QInputDialog>

#include <plog/Log.h>

#include "interface/constants/Localization.h"

#include "interface/IParentWidgetProvider.h"

using namespace HomeCompa;
using namespace Util;

Dialog::Dialog(std::shared_ptr<IParentWidgetProvider> parentProvider)
	: m_parentProvider(std::move(parentProvider))
{
	PLOGV << "Dialog created";
}

Dialog::~Dialog()
{
	PLOGV << "Dialog destroyed";
}

QMessageBox::StandardButton Dialog::Show(const QMessageBox::Icon icon, const QString & title, const QString & text, const QMessageBox::StandardButtons & buttons, const QMessageBox::StandardButton defaultButton) const
{
	auto * parent = m_parentProvider->GetWidget();
	QMessageBox msgBox(parent);
	msgBox.setFont(parent->font());
	msgBox.setIcon(icon);
	msgBox.setWindowTitle(title);
	msgBox.setTextFormat(Qt::RichText);
	msgBox.setText(text);
	msgBox.setStandardButtons(buttons);
	msgBox.setDefaultButton(defaultButton);
	return static_cast<QMessageBox::StandardButton>(msgBox.exec());
}


#define STANDARD_DIALOG_ITEM(NAME) \
NAME##Dialog::NAME##Dialog(std::shared_ptr<IParentWidgetProvider> parentProvider) \
	: Dialog(std::move(parentProvider)) {}
STANDARD_DIALOG_ITEMS_X_MACRO
#undef	STANDARD_DIALOG_ITEM

#define NO_GET_TEXT(NAME) QString NAME##Dialog::GetText(const QString & /*title*/, const QString & /*label*/, const QString & /*text*/, QLineEdit::EchoMode /*mode*/) const { throw std::runtime_error("not implemented"); }
		NO_GET_TEXT(Error)
		NO_GET_TEXT(Info)
		NO_GET_TEXT(Question)
		NO_GET_TEXT(Warning)
#undef	NO_GET_TEXT

#define NO_SHOW(NAME) QMessageBox::StandardButton NAME##Dialog::Show(const QString & /*text*/ = {}, const QMessageBox::StandardButtons & /*buttons*/ = QMessageBox::Ok, QMessageBox::StandardButton /*defaultButton*/ = QMessageBox::NoButton) const { throw std::runtime_error("not implemented"); }
		NO_SHOW(InputText)
#undef	NO_SHOW

QMessageBox::StandardButton QuestionDialog::Show(const QString & text, const QMessageBox::StandardButtons & buttons, const QMessageBox::StandardButton defaultButton) const
{
	return Dialog::Show(QMessageBox::Question, Loc::Question(), text, buttons, defaultButton);
}

QMessageBox::StandardButton WarningDialog::Show(const QString & text, const QMessageBox::StandardButtons & buttons, const QMessageBox::StandardButton defaultButton) const
{
	return Dialog::Show(QMessageBox::Warning, Loc::Warning(), text, buttons, defaultButton);
}

QMessageBox::StandardButton InfoDialog::Show(const QString & text, const QMessageBox::StandardButtons & buttons, const QMessageBox::StandardButton defaultButton) const
{
	return Dialog::Show(QMessageBox::Information, Loc::Information(), text, buttons, defaultButton);
}

QMessageBox::StandardButton ErrorDialog::Show(const QString & text, const QMessageBox::StandardButtons & buttons, const QMessageBox::StandardButton defaultButton) const
{
	return Dialog::Show(QMessageBox::Critical, Loc::Error(), text, buttons, defaultButton);
}

QString InputTextDialog::GetText(const QString & title, const QString & label, const QString & text, const QLineEdit::EchoMode mode) const
{
	auto * parent = m_parentProvider->GetWidget();
	QInputDialog inputDialog(parent);
	inputDialog.setFont(parent->font());
	inputDialog.setWindowTitle(title);
	inputDialog.setLabelText(label);
	inputDialog.setTextEchoMode(mode);
	inputDialog.setTextValue(text);
	return inputDialog.exec() == QDialog::Accepted ? inputDialog.textValue() : QString {};
}
