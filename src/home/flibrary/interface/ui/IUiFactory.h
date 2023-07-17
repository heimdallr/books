#pragma once

#include <QtWidgets/QMessageBox>

#include "fnd/memory.h"

class QDialog;
class QWidget;

namespace HomeCompa::Flibrary {

class IUiFactory  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IUiFactory() = default;

	[[nodiscard]] virtual std::shared_ptr<QWidget> CreateTreeViewWidget(enum class ItemType type) const = 0;
	[[nodiscard]] virtual std::shared_ptr<class IAddCollectionDialog> CreateAddCollectionDialog() const = 0;
	[[nodiscard]] virtual QMessageBox::StandardButton ShowWarning(const QString & title, const QString & text, QMessageBox::StandardButtons buttons = QMessageBox::Ok, QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) const = 0;

	[[nodiscard]] virtual std::shared_ptr<class AbstractTreeViewController> GetTreeViewController() const = 0;
};

}
