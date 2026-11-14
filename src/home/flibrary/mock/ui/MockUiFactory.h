#include <gmock/gmock.h>

#include "interface/ui/IUiFactory.h"

namespace HomeCompa::Flibrary::Test {

class MockUiFactory : public IUiFactory
{
	using IDataItemPtr_QObject_Map      = std::pair<IDataItem::Ptr, QObject*>;
	using VectorPairQStringBool         = std::vector<std::pair<QString, bool>>;
	using FunctionVoidOfIntBool         = std::function<void(int, bool)>;
	using VectorPairOfButtonRoleQString = std::vector<std::pair<QMessageBox::ButtonRole, QString>>;

public: // Util::IUiFactory
	MOCK_METHOD(QObject*, GetParentObject, (QObject * defaultObject), (const, noexcept, override));
	MOCK_METHOD(QWidget*, GetParentWidget, (QWidget * defaultWidget), (const, noexcept, override));
	MOCK_METHOD(int, GetParentWidgetFontSize, (), (const, noexcept, override));
	MOCK_METHOD(
		QMessageBox::ButtonRole,
		ShowCustomDialog,
		(QMessageBox::Icon icon, const QString& title, const QString& text, const VectorPairOfButtonRoleQString& buttons, QMessageBox::ButtonRole defaultButton, const QString& detailedText),
		(const, override)
	);
	MOCK_METHOD(QMessageBox::StandardButton, ShowQuestion, (Util::DialogInitializer & initializer), (const, override));
	MOCK_METHOD(QMessageBox::StandardButton, ShowWarning, (Util::DialogInitializer & initializer), (const, override));
	MOCK_METHOD(void, ShowInfo, (const QString& text), (const, override));
	MOCK_METHOD(void, ShowError, (const QString& text), (const, override));
	MOCK_METHOD(QString, GetText, (const QString& title, const QString& label, const QString& text, const QStringList& comboBoxItems, QLineEdit::EchoMode mode), (const, override));
	MOCK_METHOD(std::shared_ptr<QFont>, GetFont, (const QString& title, const QFont& font, const QFontDialog::FontDialogOptions& options), (const, override));
	MOCK_METHOD(std::optional<QColor>, GetColor, (const QString& title, const QColor& color, const QColorDialog::ColorDialogOptions& options), (const, override));
	MOCK_METHOD(QStringList, GetOpenFileNames, (const QString& key, const QString& title, const QString& filter, const QString& dir, const QFileDialog::Options& options), (const, override));
	MOCK_METHOD(QString, GetOpenFileName, (const QString& key, const QString& title, const QString& filter, const QString& dir, const QFileDialog::Options& options), (const, override));
	MOCK_METHOD(QString, GetSaveFileName, (const QString& key, const QString& title, const QString& filter, const QString& dir, const QFileDialog::Options& options), (const, override));
	MOCK_METHOD(QString, GetExistingDirectory, (const QString& key, const QString& title, const QString& dir, const QFileDialog::Options& options), (const, override));

public: // IUiFactory
	MOCK_METHOD(std::shared_ptr<TreeView>, CreateTreeViewWidget, (ItemType type), (const, override));
	MOCK_METHOD(std::shared_ptr<IAddCollectionDialog>, CreateAddCollectionDialog, (std::filesystem::path inpxFolder), (const, override));
	MOCK_METHOD(std::shared_ptr<QDialog>, CreateScriptDialog, (), (const, override));
	MOCK_METHOD(std::shared_ptr<QDialog>, CreateSettingsDialog, (), (const, override));
	MOCK_METHOD(std::shared_ptr<ITreeViewDelegate>, CreateTreeViewDelegateBooks, (QTreeView & parent), (const, override));
	MOCK_METHOD(std::shared_ptr<ITreeViewDelegate>, CreateTreeViewDelegateNavigation, (QAbstractItemView & parent), (const, override));
	MOCK_METHOD(std::shared_ptr<QDialog>, CreateOpdsDialog, (), (const, override));
	MOCK_METHOD(std::shared_ptr<QDialog>, CreateHotkeyDialog, (), (const, override));
	MOCK_METHOD(std::shared_ptr<QDialog>, CreateFilterSettingsDialog, (), (const, override));
	MOCK_METHOD(std::shared_ptr<IComboBoxTextDialog>, CreateComboBoxTextDialog, (QString title), (const, override));
	MOCK_METHOD(std::shared_ptr<QMainWindow>, CreateQueryWindow, (), (const, override));
	MOCK_METHOD(QWidget*, CreateCollectionCleaner, (QStackedWidget * stackedWidget), (const, override));
	MOCK_METHOD(QWidget*, CreateImageViewer, (QStackedWidget * stackedWidget), (const, override));
	MOCK_METHOD(void, CreateAuthorReview, (long long id), (const, override));
	MOCK_METHOD(void, ExecuteContextMenu, (QLineEdit * lineEdit), (const, override));
	MOCK_METHOD(void, ShowAbout, (), (const, override));
	MOCK_METHOD(void, UpdateRecentOpenBookControllerMenu, (QMenu & menu), (const, override));
	MOCK_METHOD(void, SetBackgroundStyleSheet, (QWidget & widget, const QString& key), (const, override));
	MOCK_METHOD(void, ExecMenu, (const QStringList& values, std::function<void(int)> callback), (const, override));
	MOCK_METHOD(IDataItemPtr_QObject_Map, AddWidgetToMenuCustomizer, (const QString& rootKey, QWidget& widget, const QString& title, const MenuCustomizeFunctor& functor), (const, override));
	MOCK_METHOD(IDataItemPtr_QObject_Map, AddMenuBarToMenuCustomizer, (const QString& rootKey, QMenuBar& menuBar, const QString& title, const MenuCustomizeFunctor& functor), (const, override));
	MOCK_METHOD(IDataItemPtr_QObject_Map, AddComboBoxToMenuCustomizer, (const QString& rootKey, QComboBox& comboBox, const QString& title, const MenuCustomizeFunctor& functor), (const, override));
	MOCK_METHOD(IMenuCustomizer::IItem::Ptr, CreateMenuCustomizerItem, (IDataItem::Ptr, IMenuCustomizer::IObserver& observer), (const, override));
	MOCK_METHOD(IMenuCustomizer::IItem::Ptr, CreateMenuCustomizerItem, (QString key), (const, override));
	MOCK_METHOD(void, SaveMenuCustomizerSettings, (), (const, override));
	MOCK_METHOD(bool, LoadMenuCustomizerSettings, (), (const, override));
	MOCK_METHOD(QWidget*, CreateFastFilterWidget, (const QAbstractItemModel& model, int column, std::function<void(bool, QVariantList)> callback), (const, override));
	MOCK_METHOD(QWidget*, CreateChangeSizeWidget, (int current, int minimum, int maximum, IChangeSizeWidgetObserver* observer), (const, override));
	MOCK_METHOD(QMenu*, CreateCheckableMenu, (const VectorPairQStringBool& values, FunctionVoidOfIntBool callback), (const, override));
	MOCK_METHOD(std::shared_ptr<QIcon>, CreateIcon, (QByteArray data), (const, override));
	MOCK_METHOD(std::filesystem::path, GetNewCollectionInpxFolder, (), (const, noexcept, override));
	MOCK_METHOD(std::shared_ptr<ITreeViewController>, GetTreeViewController, (), (const, noexcept, override));
	MOCK_METHOD(QTreeView&, GetTreeView, (), (const, override));
	MOCK_METHOD(QAbstractItemView&, GetAbstractItemView, (), (const, override));
	MOCK_METHOD(QString, GetTitle, (), (const, noexcept, override));
	MOCK_METHOD(long long, GetAuthorId, (), (const, noexcept, override));
	MOCK_METHOD(QStackedWidget*, GetStackedWidget, (), (const, noexcept, override));
};

static_assert(!std::is_abstract_v<MockUiFactory>, "MockUiFactory is abstract!");

} // namespace HomeCompa::Flibrary::Test
