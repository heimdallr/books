#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/IDialog.h"
#include "interface/IParentWidgetProvider.h"

#include "settings/ISettings.h"

class QWidget;

namespace HomeCompa
{

class IParentWidgetProvider;

}

namespace HomeCompa::Util
{

class Dialog : virtual public IDialog
{
	NON_COPY_MOVABLE(Dialog)

protected:
	Dialog(std::shared_ptr<IParentWidgetProvider> parentProvider, std::shared_ptr<ISettings> settings);
	~Dialog() override;

protected: // IDialog
	QMessageBox::StandardButton Show(QMessageBox::Icon icon, const QString& title, DialogInitializer& initializer) const override;

protected:
	PropagateConstPtr<IParentWidgetProvider, std::shared_ptr> m_parentProvider;
	std::shared_ptr<ISettings>                                m_settings;
};

#define STANDARD_DIALOG_ITEM(NAME)                                                                                                                                                       \
	class NAME##Dialog final                                                                                                                                                             \
		: public Dialog                                                                                                                                                                  \
		, public I##NAME##Dialog                                                                                                                                                         \
	{                                                                                                                                                                                    \
	public:                                                                                                                                                                              \
		NAME##Dialog(std::shared_ptr<IParentWidgetProvider> parentProvider, std::shared_ptr<ISettings> settings);                                                                        \
                                                                                                                                                                                         \
	private:                                                                                                                                                                             \
		QMessageBox::StandardButton Show(DialogInitializer& initializer) const override;                                                                                                 \
		QString                     GetText(const QString& title, const QString& label, const QString& text, const QStringList& comboBoxItems, QLineEdit::EchoMode mode) const override; \
	};
STANDARD_DIALOG_ITEMS_X_MACRO
#undef STANDARD_DIALOG_ITEM

} // namespace HomeCompa::Util
