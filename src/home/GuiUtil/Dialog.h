#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "interface/IDialog.h"

class QWidget;

namespace HomeCompa {
class IParentWidgetProvider;
}

namespace HomeCompa::Util {

class Dialog : virtual public IDialog
{
	NON_COPY_MOVABLE(Dialog)

protected:
	explicit Dialog(std::shared_ptr<IParentWidgetProvider> parentProvider);
	~Dialog() override;

protected:
	QMessageBox::StandardButton Show(QMessageBox::Icon icon, const QString & title, const QString & text, QMessageBox::StandardButtons buttons = QMessageBox::Ok, QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) const;

protected:
	PropagateConstPtr<IParentWidgetProvider, std::shared_ptr> m_parentProvider;
};

#define STANDARD_DIALOG_ITEM(NAME)                                                                                                                          \
class NAME##Dialog final                                                                                                                                    \
	: public Dialog                                                                                                                                         \
	, public I##NAME##Dialog                                                                                                                                \
{                                                                                                                                                           \
public:                                                                                                                                                     \
	explicit NAME##Dialog(std::shared_ptr<IParentWidgetProvider> parentProvider);                                                                           \
private:                                                                                                                                                    \
	QMessageBox::StandardButton Show(const QString & text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) const override; \
	QString GetText(const QString & title, const QString & label, const QString & text = {}, QLineEdit::EchoMode mode = QLineEdit::Normal) const override;  \
};
STANDARD_DIALOG_ITEMS_X_MACRO
#undef	STANDARD_DIALOG_ITEM

}
