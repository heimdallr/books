#pragma once

#include <QtWidgets/QMessageBox>

namespace HomeCompa::Flibrary {

class IDialog  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IDialog() = default;

	[[nodiscard]] virtual QMessageBox::StandardButton Show(const QString & title, const QString & text, QMessageBox::StandardButtons buttons = QMessageBox::Ok, QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) const = 0;

};

class IWarningDialog : virtual public IDialog
{
};

}
