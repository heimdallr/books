#pragma once

#include <QLineEdit>
#include <QtWidgets/QMessageBox>

namespace HomeCompa::Flibrary {

class IDialog  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IDialog() = default;

	[[nodiscard]] virtual QMessageBox::StandardButton Show(const QString & text = {}, QMessageBox::StandardButtons buttons = QMessageBox::Ok, QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) const = 0;
	[[nodiscard]] virtual QString GetText(const QString & title, const QString & label, const QString & text = {}, QLineEdit::EchoMode mode = QLineEdit::Normal) const = 0;
};

#define STANDARD_DIALOG_ITEMS_X_MACRO \
STANDARD_DIALOG_ITEM(About)           \
STANDARD_DIALOG_ITEM(Error)           \
STANDARD_DIALOG_ITEM(Info)            \
STANDARD_DIALOG_ITEM(InputText)       \
STANDARD_DIALOG_ITEM(Question)        \
STANDARD_DIALOG_ITEM(Warning)

#define STANDARD_DIALOG_ITEM(NAME) class I##NAME##Dialog : virtual public IDialog {};
		STANDARD_DIALOG_ITEMS_X_MACRO
#undef	STANDARD_DIALOG_ITEM

}
