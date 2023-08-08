#pragma once

#include <memory>

#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>

class QAbstractItemDelegate;
class QAbstractScrollArea;
class QDialog;
class QWidget;

namespace HomeCompa::Flibrary {

class IUiFactory  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IUiFactory() = default;

	[[nodiscard]] virtual std::shared_ptr<class TreeView> CreateTreeViewWidget(enum class ItemType type) const = 0;
	[[nodiscard]] virtual std::shared_ptr<class IAddCollectionDialog> CreateAddCollectionDialog() const = 0;
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemDelegate> CreateTreeViewDelegateBooks(QAbstractScrollArea & parent) const = 0;

	virtual void ShowAbout() const = 0;
	[[nodiscard]] virtual QMessageBox::StandardButton ShowQuestion(const QString & text, QMessageBox::StandardButtons buttons = QMessageBox::Ok, QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) const = 0;
	virtual QMessageBox::StandardButton ShowWarning(const QString & text, QMessageBox::StandardButtons buttons = QMessageBox::Ok, QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) const = 0;
	[[nodiscard]] virtual QString GetText(const QString & title, const QString & label, const QString & text = {}, QLineEdit::EchoMode mode = QLineEdit::Normal) const = 0;

	[[nodiscard]] virtual std::shared_ptr<class AbstractTreeViewController> GetTreeViewController() const noexcept = 0;
	[[nodiscard]] virtual QAbstractScrollArea & GetAbstractScrollArea() const noexcept = 0;
};

}
