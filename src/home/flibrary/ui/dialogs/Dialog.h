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

class AboutDialog final
	: public Dialog
	, public IAboutDialog
{
public:
	explicit AboutDialog(std::shared_ptr<ParentWidgetProvider> parentProvider);

private: // IDialog
	QMessageBox::StandardButton Show(const QString & text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) const override;
};

class QuestionDialog final
	: public Dialog
	, public IQuestionDialog
{
public:
	explicit QuestionDialog(std::shared_ptr<ParentWidgetProvider> parentProvider);

private: // IDialog
	QMessageBox::StandardButton Show(const QString & text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) const override;
};

class WarningDialog final
	: public Dialog
	, public IWarningDialog
{
public:
	explicit WarningDialog(std::shared_ptr<ParentWidgetProvider> parentProvider);

private: // IDialog
	QMessageBox::StandardButton Show(const QString & text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) const override;
};

}
