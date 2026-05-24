#include "Dialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QInputDialog>
#include <QTimer>

#include "interface/localization.h"

#include "platformgui/PlatformGuiUtil.h"
#include "utilgui/GeometryRestorable.h"

#include "QtTypes.h"
#include "log.h"

using namespace HomeCompa;
using namespace Util;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	#define CHECK_STATE_CHANGED checkStateChanged
	#define CHECK_STATE Qt::CheckState
#elif QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
	#define CHECK_STATE_CHANGED stateChanged
	#define CHECK_STATE int
#else
	#error unsupported qt version
#endif

namespace
{

constexpr auto INPUT_DIALOG_GEOMETRY_KEY = "ui/InputDialog/Geometry";

}

Dialog::Dialog(std::shared_ptr<IParentWidgetProvider> parentProvider, std::shared_ptr<ISettings> settings)
	: m_parentProvider { std::move(parentProvider) }
	, m_settings { std::move(settings) }
{
	PLOGV << "Dialog created";
}

Dialog::~Dialog()
{
	PLOGV << "Dialog destroyed";
}

QMessageBox::StandardButton Dialog::Show(const QMessageBox::Icon icon, const QString& title, DialogInitializer& initializer) const
{
	auto*       parent = m_parentProvider->GetWidget();
	QMessageBox msgBox(parent);
	msgBox.setFont(parent ? parent->font() : QApplication::font());
	msgBox.setIcon(icon);
	msgBox.setWindowTitle(title);
	msgBox.setTextFormat(Qt::RichText);
	msgBox.setText(initializer.text);
	msgBox.setStandardButtons(initializer.buttons);
	msgBox.setDefaultButton(initializer.defaultButton);

	if (!initializer.checkboxText.isEmpty())
	{
		auto* checkBox = new QCheckBox(initializer.checkboxText, &msgBox);
		msgBox.setCheckBox(checkBox);
		QObject::connect(checkBox, &QCheckBox::CHECK_STATE_CHANGED, [&](const CHECK_STATE checkState) {
			initializer.checked = static_cast<Qt::CheckState>(checkState);
		});
	}

	msgBox.show();
	MoveToParentCenter(msgBox);

	return static_cast<QMessageBox::StandardButton>(msgBox.exec());
}

#define STANDARD_DIALOG_ITEM(NAME)                                                                                         \
	NAME##Dialog::NAME##Dialog(std::shared_ptr<IParentWidgetProvider> parentProvider, std::shared_ptr<ISettings> settings) \
		: Dialog(std::move(parentProvider), std::move(settings))                                                           \
	{                                                                                                                      \
	}
STANDARD_DIALOG_ITEMS_X_MACRO
#undef STANDARD_DIALOG_ITEM

#define NO_GET_TEXT(NAME)                                                                                                                                                                \
	QString NAME##Dialog::GetText(const QString& /*title*/, const QString& /*label*/, const QString& /*text*/, const QStringList& /*comboBoxItems*/, QLineEdit::EchoMode /*mode*/) const \
	{                                                                                                                                                                                    \
		throw std::runtime_error("not implemented");                                                                                                                                     \
	}
NO_GET_TEXT(Error)
NO_GET_TEXT(Info)
NO_GET_TEXT(Question)
NO_GET_TEXT(Warning)
#undef NO_GET_TEXT

#define NO_SHOW(NAME)                                                        \
	QMessageBox::StandardButton NAME##Dialog::Show(DialogInitializer&) const \
	{                                                                        \
		throw std::runtime_error("not implemented");                         \
	}
NO_SHOW(InputText)
#undef NO_SHOW

QMessageBox::StandardButton QuestionDialog::Show(DialogInitializer& initializer) const
{
	return Dialog::Show(QMessageBox::Question, Loc::Question(), initializer);
}

QMessageBox::StandardButton WarningDialog::Show(DialogInitializer& initializer) const
{
	return Dialog::Show(QMessageBox::Warning, Loc::Warning(), initializer);
}

QMessageBox::StandardButton InfoDialog::Show(DialogInitializer& initializer) const
{
	return Dialog::Show(QMessageBox::Information, Loc::Information(), initializer);
}

QMessageBox::StandardButton ErrorDialog::Show(DialogInitializer& initializer) const
{
	return Dialog::Show(QMessageBox::Critical, Loc::Error(), initializer);
}

QString InputTextDialog::GetText(const QString& title, const QString& label, const QString& text, const QStringList& comboBoxItems, const QLineEdit::EchoMode mode) const
{
	auto*        parent = m_parentProvider->GetWidget();
	QInputDialog inputDialog(parent);
	inputDialog.setFont(parent->font());
	inputDialog.setWindowTitle(title);
	inputDialog.setLabelText(label);
	inputDialog.setTextEchoMode(mode);
	inputDialog.setTextValue(text);

	if (!comboBoxItems.isEmpty())
		inputDialog.setComboBoxItems(comboBoxItems);

	QObject::connect(&inputDialog, &QDialog::finished, &inputDialog, [&] {
		m_settings->Set(INPUT_DIALOG_GEOMETRY_KEY, inputDialog.geometry());
	});
	QTimer::singleShot(0, [&] {
		if (auto geometry = m_settings->Get(INPUT_DIALOG_GEOMETRY_KEY); geometry.isValid())
			Platform::SetGeometry(inputDialog, geometry.toRect());
	});
	return inputDialog.exec() == QDialog::Accepted ? inputDialog.textValue() : QString {};
}
