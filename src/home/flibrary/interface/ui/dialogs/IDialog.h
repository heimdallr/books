#pragma once

#include <QtWidgets/QMessageBox>

namespace HomeCompa::Flibrary {

class IDialog  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IDialog() = default;

	[[nodiscard]] virtual QMessageBox::StandardButton Show(const QString & /*title*/, const QString & /*text*/, QMessageBox::StandardButtons /*buttons*/ = QMessageBox::Ok, QMessageBox::StandardButton /*defaultButton*/ = QMessageBox::NoButton) const
	{
		assert(false && "No impl");
		throw std::runtime_error("No impl");
	}
	[[nodiscard]] virtual QMessageBox::StandardButton Show(const QString & /*text*/, QMessageBox::StandardButtons /*buttons*/ = QMessageBox::Ok, QMessageBox::StandardButton /*defaultButton*/ = QMessageBox::NoButton) const
	{
		assert(false && "No impl");
		throw std::runtime_error("No impl");
	}

};

class IQuestionDialog : virtual public IDialog
{
};

class IWarningDialog : virtual public IDialog
{
};

}
