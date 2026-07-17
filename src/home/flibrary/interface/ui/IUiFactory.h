#pragma once

#include <filesystem>

#include "interface/constants/Enums.h"
#include "interface/logic/IDataItem.h"

#include "gutil/interface/IUiFactory.h"

class QStackedWidget;
class QAbstractItemModel;
class QAbstractItemView;
class QComboBox;
class QDialog;
class QMainWindow;
class QMenuBar;
class QShortcut;
class QTreeView;

namespace HomeCompa
{

class ISettings;

}

namespace HomeCompa::Flibrary
{

class IUiFactory : virtual public Util::IUiFactory // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IChangeSizeWidgetObserver // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~IChangeSizeWidgetObserver() = default;
		virtual void OnSizeChanged(int size) = 0;
	};

public:
	[[nodiscard]] virtual std::shared_ptr<class TreeView>             CreateTreeViewWidget(ItemType type) const                         = 0;
	[[nodiscard]] virtual std::shared_ptr<class IAddCollectionDialog> CreateAddCollectionDialog(std::filesystem::path inpxFolder) const = 0;
	[[nodiscard]] virtual std::shared_ptr<QDialog>                    CreateScriptDialog() const                                        = 0;
	[[nodiscard]] virtual std::shared_ptr<QDialog>                    CreateSettingsDialog() const                                      = 0;
	[[nodiscard]] virtual std::shared_ptr<class ITreeViewDelegate>    CreateTreeViewDelegateBooks(QTreeView& parent) const              = 0;
	[[nodiscard]] virtual std::shared_ptr<class ITreeViewDelegate>    CreateTreeViewDelegateNavigation(QAbstractItemView& parent) const = 0;
	[[nodiscard]] virtual std::shared_ptr<QDialog>                    CreateOpdsDialog() const                                          = 0;
	[[nodiscard]] virtual std::shared_ptr<QDialog>                    CreateHotkeyDialog() const                                        = 0;
	[[nodiscard]] virtual std::shared_ptr<QDialog>                    CreateFilterSettingsDialog() const                                = 0;
	[[nodiscard]] virtual std::shared_ptr<class IComboBoxTextDialog>  CreateComboBoxTextDialog(QString title) const                     = 0;
	[[nodiscard]] virtual std::shared_ptr<QMainWindow>                CreateQueryWindow() const                                         = 0;
	virtual QWidget*                                                  CreateCollectionCleaner(QStackedWidget* stackedWidget) const      = 0;
	virtual QWidget*                                                  CreateImageViewer(QStackedWidget* stackedWidget) const            = 0;
	virtual void                                                      CreateAuthorReview(long long id) const                            = 0;
	virtual void                                                      ExecuteContextMenu(QLineEdit* lineEdit) const                     = 0;
	virtual void                                                      ShowAbout() const                                                 = 0;
	virtual void                                                      UpdateRecentOpenBookControllerMenu(QMenu& menu) const             = 0;

	[[nodiscard]] virtual IDataItem::Ptr
	AddMenuBarToHotkeys(const ISettings& settings, const QMenuBar& menuBar, const QString& title, const std::function<void(const IDataItem::Ptr&, QAction*)>& functor) const = 0;
	[[nodiscard]] virtual IDataItem::Ptr
	AddComboBoxToHotkeys(const ISettings& settings, QComboBox& comboBox, const QString& title, const std::function<void(const IDataItem::Ptr&, QShortcut*)>& functor) const = 0;

	[[nodiscard]] virtual QWidget* CreateFastFilterWidget(const QAbstractItemModel& model, int column, std::function<void(bool, QVariantList)> callback) const = 0;
	[[nodiscard]] virtual QWidget* CreateChangeSizeWidget(int current, int minimum, int maximum, IChangeSizeWidgetObserver* observer) const                    = 0;

public: // special
	[[nodiscard]] virtual std::filesystem::path                      GetNewCollectionInpxFolder() const noexcept = 0;
	[[nodiscard]] virtual std::shared_ptr<class ITreeViewController> GetTreeViewController() const noexcept      = 0;
	[[nodiscard]] virtual QTreeView&                                 GetTreeView() const                         = 0;
	[[nodiscard]] virtual QAbstractItemView&                         GetAbstractItemView() const                 = 0;
	[[nodiscard]] virtual QString                                    GetTitle() const noexcept                   = 0;
	[[nodiscard]] virtual long long                                  GetAuthorId() const noexcept                = 0;
	[[nodiscard]] virtual QStackedWidget*                            GetStackedWidget() const noexcept           = 0;
};

} // namespace HomeCompa::Flibrary
