#include "di_app.h"

#include "interface/logic/ICollectionProvider.h"
#include "interface/ui/IUiFactory.h"

#include "Hypodermic/Hypodermic.h"
#include "logic/di_logic.h"
#include "requester/NoSqlRequester.h"
#include "requester/ReactAppRequester.h"
#include "requester/Requester.h"
#include "util/CoverCache.h"

#include "Server.h"

namespace HomeCompa::Opds
{

namespace
{

class UiFactory final : public Flibrary::IUiFactory
{
	QString GetExistingDirectory(const QString& /*key*/, const QString& /*title*/, const QString& /*dir*/, const QFileDialog::Options& /*options*/) const override
	{
		return {};
	}

	std::optional<QFont> GetFont(const QString& /*title*/, const QFont& /*font*/, const QFontDialog::FontDialogOptions& /*options*/) const override
	{
		return {};
	}

	QString GetOpenFileName(const QString& /*key*/, const QString& /*title*/, const QString& /*filter*/, const QString& /*dir*/, const QFileDialog::Options& /*options*/) const override
	{
		return {};
	}

	QStringList GetOpenFileNames(const QString& /*key*/, const QString& /*title*/, const QString& /*filter*/, const QString& /*dir*/, const QFileDialog::Options& /*options*/) const override
	{
		return {};
	}

	QObject* GetParentObject(QObject* /*defaultObject*/) const noexcept override
	{
		return {};
	}

	QWidget* GetParentWidget(QWidget* /*defaultWidget*/) const noexcept override
	{
		return {};
	}

	int GetParentWidgetFontSize() const noexcept override
	{
		return {};
	}

	QString GetSaveFileName(const QString& /*key*/, const QString& /*title*/, const QString& /*filter*/, const QString& /*dir*/, const QFileDialog::Options& /*options*/) const override
	{
		return {};
	}

	QString GetText(const QString& /*title*/, const QString& /*label*/, const QString& /*text*/, const QStringList& /*comboBoxItems*/, QLineEdit::EchoMode /*mode*/) const override
	{
		return {};
	}

	QMessageBox::ButtonRole ShowCustomDialog(
		QMessageBox::Icon /*icon*/,
		const QString& /*title*/,
		const QString& /*text*/,
		const std::vector<std::pair<QMessageBox::ButtonRole, QString>>& /*buttons*/,
		QMessageBox::ButtonRole /*defaultButton*/,
		const QString& /*detailedText*/
	) const override
	{
		return {};
	}

	void ShowError(const QString& /*text*/) const override
	{
	}

	void ShowInfo(const QString& /*text*/) const override
	{
	}

	QMessageBox::StandardButton ShowQuestion(Util::DialogInitializer&) const override
	{
		return {};
	}

	QMessageBox::StandardButton ShowWarning(Util::DialogInitializer&) const override
	{
		return {};
	}

private: // IUiFactory
	std::shared_ptr<Flibrary::TreeView> CreateTreeViewWidget(Flibrary::ItemType) const override
	{
		return {};
	}

	std::shared_ptr<Flibrary::IAddCollectionDialog> CreateAddCollectionDialog(std::filesystem::path) const override
	{
		return {};
	}

	std::shared_ptr<QDialog> CreateScriptDialog() const override
	{
		return {};
	}

	std::shared_ptr<QDialog> CreateSettingsDialog() const override
	{
		return {};
	}

	std::shared_ptr<Flibrary::ITreeViewDelegate> CreateTreeViewDelegateBooks(QTreeView&) const override
	{
		return {};
	}

	std::shared_ptr<Flibrary::ITreeViewDelegate> CreateTreeViewDelegateNavigation(QAbstractItemView&) const override
	{
		return {};
	}

	std::shared_ptr<QDialog> CreateOpdsDialog() const override
	{
		return {};
	}

	std::shared_ptr<QDialog> CreateHotkeyDialog() const override
	{
		return {};
	}

	std::shared_ptr<QDialog> CreateFilterSettingsDialog() const override
	{
		return {};
	}

	std::shared_ptr<Flibrary::IComboBoxTextDialog> CreateComboBoxTextDialog(QString) const override
	{
		return {};
	}

	std::shared_ptr<QMainWindow> CreateQueryWindow() const override
	{
		return {};
	}

	void CreateCollectionCleaner() const override
	{
	}

	void CreateAuthorReview(long long /*id*/) const override
	{
	}

	void ExecuteContextMenu(QLineEdit* /*lineEdit*/) const override
	{
	}

	void ShowAbout() const override
	{
	}

	void UpdateRecentOpenBookControllerMenu(QMenu& /*menu*/) const override
	{
	}

	Flibrary::IDataItem::Ptr
	AddMenuBarToHotkeys(const ISettings& /*settings*/, const QMenuBar& /*menuBar*/, const QString& /*title*/, const std::function<void(const Flibrary::IDataItem::Ptr&, QAction*)>& /*functor*/) const override
	{
		return {};
	}

	Flibrary::IDataItem::Ptr
	AddComboBoxToHotkeys(const ISettings& /*settings*/, QComboBox& /*comboBox*/, const QString& /*title*/, const std::function<void(const Flibrary::IDataItem::Ptr&, QShortcut*)>& /*functor*/) const override
	{
		return {};
	}

public: // special
	std::filesystem::path GetNewCollectionInpxFolder() const noexcept override
	{
		return {};
	}

	std::shared_ptr<Flibrary::ITreeViewController> GetTreeViewController() const noexcept override
	{
		return {};
	}

	QTreeView& GetTreeView() const override
	{
		throw std::runtime_error("Not implemented");
	}

	QAbstractItemView& GetAbstractItemView() const override
	{
		throw std::runtime_error("Not implemented");
	}

	QString GetTitle() const noexcept override
	{
		return {};
	}

	long long GetAuthorId() const noexcept override
	{
		return {};
	}
};

} // namespace

void DiInit(Hypodermic::ContainerBuilder& builder, std::shared_ptr<Hypodermic::Container>& container)
{
	Flibrary::DiLogic(builder, container);

	builder.registerType<CoverCache>().as<ICoverCache>().singleInstance();
	builder.registerType<NoSqlRequester>().as<INoSqlRequester>().singleInstance();
	builder.registerType<ReactAppRequester>().as<IReactAppRequester>().singleInstance();
	builder.registerType<Requester>().as<IRequester>().singleInstance();
	builder.registerType<Server>().as<IServer>().singleInstance();

	builder
		.registerInstanceFactory([](auto&) {
			return std::make_shared<UiFactory>();
		})
		.as<Flibrary::IUiFactory>()
		.singleInstance();

	container = builder.build();
}

} // namespace HomeCompa::Opds
