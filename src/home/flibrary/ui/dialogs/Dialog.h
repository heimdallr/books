#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "interface/ui/dialogs/IDialog.h"

class QWidget;

namespace HomeCompa::Flibrary {

class Dialog : virtual public IDialog
{
	NON_COPY_MOVABLE(Dialog)

protected:
	explicit Dialog(std::shared_ptr<class ParentWidgetProvider> parentProvider);
	~Dialog() override;

protected:
	PropagateConstPtr<ParentWidgetProvider, std::shared_ptr> m_parentProvider;
};

#define STANDARD_DIALOG_ITEM(NAME)                                                                                                                          \
class NAME##Dialog final                                                                                                                                    \
	: public Dialog                                                                                                                                         \
	, public I##NAME##Dialog                                                                                                                                \
{                                                                                                                                                           \
public:                                                                                                                                                     \
	explicit NAME##Dialog(std::shared_ptr<ParentWidgetProvider> parentProvider);                                                                            \
private:                                                                                                                                                    \
	QMessageBox::StandardButton Show(const QString & text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) const override; \
	QString GetText(const QString & title, const QString & label, const QString & text = {}, QLineEdit::EchoMode mode = QLineEdit::Normal) const override;  \
};
STANDARD_DIALOG_ITEMS_X_MACRO
#undef	STANDARD_DIALOG_ITEM

}
