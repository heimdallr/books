#include "ui_SettingsDialog.h"

#include "SettingsDialog.h"

#include <ranges>

#include <QAbstractButton>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QIdentityProxyModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QSpinBox>
#include <QVBoxLayout>

#include "interface/constants/SettingsConstant.h"
#include "interface/localization.h"

#include "gutil/util.h"
#include "logic/data/DataItem.h"
#include "utilgui/GeometryRestorable.h"

#include "config/version.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace
{

constexpr auto CONTEXT = "SettingsDialog";
constexpr auto KEY     = QT_TRANSLATE_NOOP("SettingsDialog", "Key");
constexpr auto VALUE   = QT_TRANSLATE_NOOP("SettingsDialog", "Value");
constexpr auto REMOVE  = QT_TRANSLATE_NOOP("SettingsDialog", "Remove");

constexpr auto SETTINGS                = QT_TRANSLATE_NOOP("SettingsDialog", "Settings");
constexpr auto APPLICATION             = QT_TRANSLATE_NOOP("SettingsDialog", "Application");
constexpr auto APPLICATION_DESCRIPTION = QT_TRANSLATE_NOOP("SettingsDialog", "File handling and window behavior");
constexpr auto FILES_AND_PATHS         = QT_TRANSLATE_NOOP("SettingsDialog", "Files and paths");
constexpr auto RELATIVE_PATHS          = QT_TRANSLATE_NOOP("SettingsDialog", "Prefer relative paths when possible");
constexpr auto WINDOW_BEHAVIOR         = QT_TRANSLATE_NOOP("SettingsDialog", "Window behavior");
constexpr auto HIDE_TO_TRAY            = QT_TRANSLATE_NOOP("SettingsDialog", "Keep FLibrary in the system tray after closing the window");
constexpr auto MINIMIZE_TO_TRAY        = QT_TRANSLATE_NOOP("SettingsDialog", "Send FLibrary to the system tray when minimized");
constexpr auto BOOK_LIST               = QT_TRANSLATE_NOOP("SettingsDialog", "Book list");
constexpr auto BOOK_LIST_DESCRIPTION   = QT_TRANSLATE_NOOP("SettingsDialog", "Settings for the catalog table and quick search");
constexpr auto TABLE                   = QT_TRANSLATE_NOOP("SettingsDialog", "Table");
constexpr auto ALTERNATING_ROWS        = QT_TRANSLATE_NOOP("SettingsDialog", "Use alternating row colors");
constexpr auto QUICK_SEARCH            = QT_TRANSLATE_NOOP("SettingsDialog", "Quick search fields");
constexpr auto SEARCH_TITLE            = QT_TRANSLATE_NOOP("SettingsDialog", "Title");
constexpr auto SEARCH_AUTHOR           = QT_TRANSLATE_NOOP("SettingsDialog", "Author");
constexpr auto SEARCH_SERIES           = QT_TRANSLATE_NOOP("SettingsDialog", "Series");
constexpr auto SEARCH_ANNOTATION       = QT_TRANSLATE_NOOP("SettingsDialog", "Annotation");
constexpr auto READER                  = QT_TRANSLATE_NOOP("SettingsDialog", "Reader");
constexpr auto READER_DESCRIPTION      = QT_TRANSLATE_NOOP("SettingsDialog", "External applications used to open book formats");
constexpr auto EXTERNAL_APPLICATIONS   = QT_TRANSLATE_NOOP("SettingsDialog", "External applications");
constexpr auto DEFAULT_READER_HINT     = QT_TRANSLATE_NOOP("SettingsDialog", "Use \"default\" for the system application");
constexpr auto NETWORK                 = QT_TRANSLATE_NOOP("SettingsDialog", "Network");
constexpr auto NETWORK_DESCRIPTION     = QT_TRANSLATE_NOOP("SettingsDialog", "OPDS and browser access settings");
constexpr auto SERVER                  = QT_TRANSLATE_NOOP("SettingsDialog", "Server");
constexpr auto SERVER_PORT             = QT_TRANSLATE_NOOP("SettingsDialog", "Port");
constexpr auto SIMPLE_WEB              = QT_TRANSLATE_NOOP("SettingsDialog", "Enable the simple web interface");
constexpr auto REACT_WEB               = QT_TRANSLATE_NOOP("SettingsDialog", "Enable the React web interface");
constexpr auto OPDS_CATALOG            = QT_TRANSLATE_NOOP("SettingsDialog", "Enable the OPDS catalog");
constexpr auto COLLECTION_UPDATES      = QT_TRANSLATE_NOOP("SettingsDialog", "Collection updates");
constexpr auto OPDS_AUTOUPDATE         = QT_TRANSLATE_NOOP("SettingsDialog", "Update the collection automatically for OPDS");
constexpr auto SERVER_RESTART_HINT     = QT_TRANSLATE_NOOP("SettingsDialog", "Server changes take effect after the server is restarted.");
constexpr auto ADVANCED                = QT_TRANSLATE_NOOP("SettingsDialog", "Advanced");
constexpr auto ADVANCED_DESCRIPTION    = QT_TRANSLATE_NOOP("SettingsDialog", "Stored application keys. Remove only values you understand.");
constexpr auto FIELD_WIDTH_KEY         = "ui/View/SettingsDialog/columnWidths";

TR_DEF

IDataItem::Ptr CreateModelData(const ISettings& settings)
{
	auto root = SettingsItem::Create();
	root->SetData(Tr(KEY), SettingsItem::Column::Key);
	root->SetData(Tr(VALUE), SettingsItem::Column::Value);

	const auto enumerate = [&](IDataItem& parent, const auto& r) -> void {
		for (const auto& group : settings.GetGroups())
		{
			auto child = SettingsItem::Create();
			child->SetData(group, SettingsItem::Column::Key);
			SettingsGroup settingsGroup(settings, group);
			r(*child, r);
			parent.AppendChild(std::move(child));
		}

		for (const auto& key : settings.GetKeys())
		{
			auto child = SettingsItem::Create();
			child->SetData(key, SettingsItem::Column::Key);
			child->SetData(settings.Get(key).toString(), SettingsItem::Column::Value);
			parent.AppendChild(std::move(child));
		}
	};

	enumerate(*root, enumerate);
	return root;
}

QString GetKey(QModelIndex index)
{
	QStringList result;
	for (; index.isValid(); index = index.parent())
		result.push_front(index.data().toString());
	return result.join('/');
}

class Model final : public QIdentityProxyModel
{
public:
	static std::unique_ptr<QAbstractItemModel> Create(const IModelProvider& modelProvider, const ISettings& settings)
	{
		return std::make_unique<Model>(modelProvider.CreateTreeModel(CreateModelData(settings)));
	}

	explicit Model(std::shared_ptr<QAbstractItemModel> source, QObject* parent = nullptr)
		: QIdentityProxyModel(parent)
		, m_source { std::move(source) }
	{
		QIdentityProxyModel::setSourceModel(m_source.get());
	}

private: // QAbstractItemModel
	[[nodiscard]] int columnCount(const QModelIndex&) const override
	{
		return 2;
	}

private:
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_source;
};

QGroupBox* AddGroup(QVBoxLayout& page, const QString& title)
{
	auto* group = new QGroupBox(title);
	group->setProperty("settingsGroup", true);
	auto* layout = new QVBoxLayout(group);
	layout->setContentsMargins(14, 12, 14, 12);
	layout->setSpacing(10);
	page.addWidget(group);
	return group;
}

void AddPageHeader(QVBoxLayout& page, const QString& title, const QString& description)
{
	auto* titleLabel = new QLabel(title);
	titleLabel->setObjectName(QStringLiteral("settingsPageTitle"));
	page.addWidget(titleLabel);

	auto* descriptionLabel = new QLabel(description);
	descriptionLabel->setProperty("secondaryText", true);
	descriptionLabel->setWordWrap(true);
	page.addWidget(descriptionLabel);
	page.addSpacing(10);
}

QCheckBox* AddCheckBox(QGroupBox& group, const QString& text)
{
	auto* checkBox = new QCheckBox(text, &group);
	group.layout()->addWidget(checkBox);
	return checkBox;
}

QLineEdit* AddLineEdit(QGroupBox& group, const QString& title)
{
	auto* row = new QWidget(&group);
	row->setProperty("settingsRow", true);
	auto* layout = new QHBoxLayout(row);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(16);
	layout->addWidget(new QLabel(title, row));
	layout->addStretch();
	auto* lineEdit = new QLineEdit(row);
	lineEdit->setMinimumWidth(230);
	layout->addWidget(lineEdit);
	group.layout()->addWidget(row);
	return lineEdit;
}

QSpinBox* AddSpinBox(QGroupBox& group, const QString& title)
{
	auto* row = new QWidget(&group);
	row->setProperty("settingsRow", true);
	auto* layout = new QHBoxLayout(row);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(16);
	layout->addWidget(new QLabel(title, row));
	layout->addStretch();
	auto* spinBox = new QSpinBox(row);
	spinBox->setRange(1024, 49151);
	spinBox->setMinimumWidth(110);
	layout->addWidget(spinBox);
	group.layout()->addWidget(row);
	return spinBox;
}

} // namespace

class SettingsDialog::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(
		QDialog&                                   self,
		const IModelProvider&                      modelProvider,
		std::shared_ptr<ISettings>                 settings,
		std::shared_ptr<Util::ItemViewToolTipper>  itemViewToolTipper,
		std::shared_ptr<Util::ScrollBarController> scrollBarController
	)
		: GeometryRestorable(*this, settings, CONTEXT)
		, GeometryRestorableObserver(self)
		, m_self { self }
		, m_settings { std::move(settings) }
		, m_model { Model::Create(modelProvider, *m_settings) }
		, m_itemViewToolTipper { std::move(itemViewToolTipper) }
		, m_scrollBarController { std::move(scrollBarController) }
	{
		m_ui.setupUi(&self);
		m_self.setWindowTitle(Tr(SETTINGS));
		SetupPages();
		LoadControlValues();

		m_itemViewToolTipper->SetScrollArea(m_ui.view);
		m_scrollBarController->SetScrollArea(m_ui.view);
		m_ui.view->setModel(m_model.get());
		m_ui.view->header()->setDefaultAlignment(Qt::AlignCenter);
		m_ui.view->setAlternatingRowColors(m_settings->Get(Constant::Settings::PREFER_ALTERNATING_ROW_COLORS, false));

		connect(&self, &QDialog::accepted, &self, [this] {
			SaveControlValues();
			RemoveImpl();
		});
		connect(m_ui.categories, &QListWidget::currentRowChanged, m_ui.pages, &QStackedWidget::setCurrentIndex);
		connect(m_ui.buttonBox, &QDialogButtonBox::clicked, &self, [this](QAbstractButton* button) {
			if (m_ui.buttonBox->standardButton(button) == QDialogButtonBox::RestoreDefaults)
				SetControlDefaults();
		});
		connect(m_ui.view, &QWidget::customContextMenuRequested, &self, [this] {
			CreateContextMenu();
		});

		m_ui.categories->setCurrentRow(0);
		LoadGeometry();
		Util::LoadHeaderSectionWidth(*m_ui.view->header(), *m_settings, FIELD_WIDTH_KEY);
	}

	~Impl() override
	{
		Util::SaveHeaderSectionWidth(*m_ui.view->header(), *m_settings, FIELD_WIDTH_KEY);
		SaveGeometry();
	}

private:
	void SetupPages()
	{
		for (const auto* title : { APPLICATION, BOOK_LIST, READER, NETWORK, ADVANCED })
			m_ui.categories->addItem(Tr(title));
		m_ui.versionLabel->setText(QStringLiteral("%1 %2").arg(PRODUCT_ID, PRODUCT_VERSION));
		m_ui.versionLabel->setProperty("tertiaryText", true);
		m_ui.advancedDescription->setText(Tr(ADVANCED_DESCRIPTION));
		m_ui.advancedDescription->setProperty("secondaryText", true);

		AddPageHeader(*m_ui.applicationPageLayout, Tr(APPLICATION), Tr(APPLICATION_DESCRIPTION));
		auto* paths      = AddGroup(*m_ui.applicationPageLayout, Tr(FILES_AND_PATHS));
		m_relativePaths  = AddCheckBox(*paths, Tr(RELATIVE_PATHS));
		auto* window     = AddGroup(*m_ui.applicationPageLayout, Tr(WINDOW_BEHAVIOR));
		m_hideToTray     = AddCheckBox(*window, Tr(HIDE_TO_TRAY));
		m_minimizeToTray = AddCheckBox(*window, Tr(MINIMIZE_TO_TRAY));
		m_ui.applicationPageLayout->addStretch();

		AddPageHeader(*m_ui.bookListPageLayout, Tr(BOOK_LIST), Tr(BOOK_LIST_DESCRIPTION));
		auto* table        = AddGroup(*m_ui.bookListPageLayout, Tr(TABLE));
		m_alternatingRows  = AddCheckBox(*table, Tr(ALTERNATING_ROWS));
		auto* search       = AddGroup(*m_ui.bookListPageLayout, Tr(QUICK_SEARCH));
		m_searchTitle      = AddCheckBox(*search, Tr(SEARCH_TITLE));
		m_searchAuthor     = AddCheckBox(*search, Tr(SEARCH_AUTHOR));
		m_searchSeries     = AddCheckBox(*search, Tr(SEARCH_SERIES));
		m_searchAnnotation = AddCheckBox(*search, Tr(SEARCH_ANNOTATION));
		m_ui.bookListPageLayout->addStretch();

		AddPageHeader(*m_ui.readerPageLayout, Tr(READER), Tr(READER_DESCRIPTION));
		auto* readers    = AddGroup(*m_ui.readerPageLayout, Tr(EXTERNAL_APPLICATIONS));
		m_readerFb2      = AddLineEdit(*readers, QStringLiteral("FB2"));
		m_readerEpub     = AddLineEdit(*readers, QStringLiteral("EPUB"));
		m_readerPdf      = AddLineEdit(*readers, QStringLiteral("PDF"));
		auto* readerHint = new QLabel(Tr(DEFAULT_READER_HINT));
		readerHint->setProperty("tertiaryText", true);
		m_ui.readerPageLayout->addWidget(readerHint);
		m_ui.readerPageLayout->addStretch();

		AddPageHeader(*m_ui.networkPageLayout, Tr(NETWORK), Tr(NETWORK_DESCRIPTION));
		auto* server     = AddGroup(*m_ui.networkPageLayout, Tr(SERVER));
		m_serverPort     = AddSpinBox(*server, Tr(SERVER_PORT));
		m_simpleWeb      = AddCheckBox(*server, Tr(SIMPLE_WEB));
		m_reactWeb       = AddCheckBox(*server, Tr(REACT_WEB));
		m_opdsCatalog    = AddCheckBox(*server, Tr(OPDS_CATALOG));
		auto* updates    = AddGroup(*m_ui.networkPageLayout, Tr(COLLECTION_UPDATES));
		m_opdsAutoUpdate = AddCheckBox(*updates, Tr(OPDS_AUTOUPDATE));
		auto* serverHint = new QLabel(Tr(SERVER_RESTART_HINT));
		serverHint->setProperty("tertiaryText", true);
		m_ui.networkPageLayout->addWidget(serverHint);
		m_ui.networkPageLayout->addStretch();
	}

	void LoadControlValues() const
	{
		m_relativePaths->setChecked(m_settings->Get(Constant::Settings::PREFER_RELATIVE_PATHS, false));
		m_hideToTray->setChecked(m_settings->Get(Constant::Settings::PREFER_HIDE_TO_TRAY_KEY, false));
		m_minimizeToTray->setChecked(m_settings->Get(Constant::Settings::PREFER_MINIMIZE_TO_TRAY_KEY, false));
		m_alternatingRows->setChecked(m_settings->Get(Constant::Settings::PREFER_ALTERNATING_ROW_COLORS, false));
		m_searchTitle->setChecked(m_settings->Get(Constant::Settings::SEARCH_WITH_TITLE, true));
		m_searchAuthor->setChecked(m_settings->Get(Constant::Settings::SEARCH_WITH_AUTHOR, true));
		m_searchSeries->setChecked(m_settings->Get(Constant::Settings::SEARCH_WITH_SERIES, true));
		m_searchAnnotation->setChecked(m_settings->Get(Constant::Settings::SEARCH_WITH_ANNOTATION, false));
		m_readerFb2->setText(m_settings->Get(QStringLiteral("Reader/fb2"), QStringLiteral("default")));
		m_readerEpub->setText(m_settings->Get(QStringLiteral("Reader/epub"), QStringLiteral("default")));
		m_readerPdf->setText(m_settings->Get(QStringLiteral("Reader/pdf"), QStringLiteral("default")));
		m_serverPort->setValue(m_settings->Get(Constant::Settings::OPDS_PORT_KEY, Constant::Settings::OPDS_PORT_DEFAULT));
		m_simpleWeb->setChecked(m_settings->Get(Constant::Settings::OPDS_WEB_ENABLED, true));
		m_reactWeb->setChecked(m_settings->Get(Constant::Settings::OPDS_REACT_APP_ENABLED, true));
		m_opdsCatalog->setChecked(m_settings->Get(Constant::Settings::OPDS_OPDS_ENABLED, true));
		m_opdsAutoUpdate->setChecked(m_settings->Get(Constant::Settings::PREFER_OPDS_AUTOUPDATE_COLLECTION, false));
	}

	void SetControlDefaults() const
	{
		m_relativePaths->setChecked(false);
		m_hideToTray->setChecked(false);
		m_minimizeToTray->setChecked(false);
		m_alternatingRows->setChecked(false);
		m_searchTitle->setChecked(true);
		m_searchAuthor->setChecked(true);
		m_searchSeries->setChecked(true);
		m_searchAnnotation->setChecked(false);
		m_readerFb2->setText(QStringLiteral("default"));
		m_readerEpub->setText(QStringLiteral("default"));
		m_readerPdf->setText(QStringLiteral("default"));
		m_serverPort->setValue(Constant::Settings::OPDS_PORT_DEFAULT);
		m_simpleWeb->setChecked(true);
		m_reactWeb->setChecked(true);
		m_opdsCatalog->setChecked(true);
		m_opdsAutoUpdate->setChecked(false);
	}

	void SaveControlValues()
	{
		m_settings->Set(Constant::Settings::PREFER_RELATIVE_PATHS, m_relativePaths->isChecked());
		m_settings->Set(Constant::Settings::PREFER_HIDE_TO_TRAY_KEY, m_hideToTray->isChecked());
		m_settings->Set(Constant::Settings::PREFER_MINIMIZE_TO_TRAY_KEY, m_minimizeToTray->isChecked());
		m_settings->Set(Constant::Settings::PREFER_ALTERNATING_ROW_COLORS, m_alternatingRows->isChecked());
		m_settings->Set(Constant::Settings::SEARCH_WITH_TITLE, m_searchTitle->isChecked());
		m_settings->Set(Constant::Settings::SEARCH_WITH_AUTHOR, m_searchAuthor->isChecked());
		m_settings->Set(Constant::Settings::SEARCH_WITH_SERIES, m_searchSeries->isChecked());
		m_settings->Set(Constant::Settings::SEARCH_WITH_ANNOTATION, m_searchAnnotation->isChecked());
		m_settings->Set(QStringLiteral("Reader/fb2"), ReaderValue(*m_readerFb2));
		m_settings->Set(QStringLiteral("Reader/epub"), ReaderValue(*m_readerEpub));
		m_settings->Set(QStringLiteral("Reader/pdf"), ReaderValue(*m_readerPdf));
		m_settings->Set(Constant::Settings::OPDS_PORT_KEY, m_serverPort->value());
		m_settings->Set(Constant::Settings::OPDS_WEB_ENABLED, m_simpleWeb->isChecked());
		m_settings->Set(Constant::Settings::OPDS_REACT_APP_ENABLED, m_reactWeb->isChecked());
		m_settings->Set(Constant::Settings::OPDS_OPDS_ENABLED, m_opdsCatalog->isChecked());
		m_settings->Set(Constant::Settings::PREFER_OPDS_AUTOUPDATE_COLLECTION, m_opdsAutoUpdate->isChecked());
	}

	static QString ReaderValue(const QLineEdit& lineEdit)
	{
		const auto value = lineEdit.text().trimmed();
		return value.isEmpty() ? QStringLiteral("default") : value;
	}

	void CreateContextMenu()
	{
		QMenu menu;
		connect(menu.addAction(Tr(REMOVE)), &QAction::triggered, &m_self, [this] {
			const auto indices = m_ui.view->selectionModel()->selectedIndexes() | std::views::filter([](const auto& item) {
									 return item.column() == 0;
								 })
			                   | std::ranges::to<std::vector<QPersistentModelIndex>>();
			for (const auto& index : indices)
			{
				if (!index.isValid())
					continue;

				m_keysToRemove << GetKey(index);
				m_model->removeRow(index.row(), index.parent());
			}
		});
		menu.setFont(m_self.font());
		Util::FillTreeContextMenu(*m_ui.view, menu).exec(QCursor::pos());
	}

	void RemoveImpl()
	{
		for (const auto& key : m_keysToRemove)
			m_settings->Remove(key);
	}

private:
	QWidget&                                                      m_self;
	Ui::SettingsDialog                                            m_ui;
	PropagateConstPtr<ISettings, std::shared_ptr>                 m_settings;
	PropagateConstPtr<QAbstractItemModel>                         m_model;
	PropagateConstPtr<Util::ItemViewToolTipper, std::shared_ptr>  m_itemViewToolTipper;
	PropagateConstPtr<Util::ScrollBarController, std::shared_ptr> m_scrollBarController;

	QCheckBox*  m_relativePaths {};
	QCheckBox*  m_hideToTray {};
	QCheckBox*  m_minimizeToTray {};
	QCheckBox*  m_alternatingRows {};
	QCheckBox*  m_searchTitle {};
	QCheckBox*  m_searchAuthor {};
	QCheckBox*  m_searchSeries {};
	QCheckBox*  m_searchAnnotation {};
	QLineEdit*  m_readerFb2 {};
	QLineEdit*  m_readerEpub {};
	QLineEdit*  m_readerPdf {};
	QSpinBox*   m_serverPort {};
	QCheckBox*  m_simpleWeb {};
	QCheckBox*  m_reactWeb {};
	QCheckBox*  m_opdsCatalog {};
	QCheckBox*  m_opdsAutoUpdate {};
	QStringList m_keysToRemove;
};

SettingsDialog::SettingsDialog(
	const std::shared_ptr<IParentWidgetProvider>& parentWidgetProvider,
	const std::shared_ptr<IModelProvider>&        modelProvider,
	std::shared_ptr<ISettings>                    settings,
	std::shared_ptr<Util::ItemViewToolTipper>     itemViewToolTipper,
	std::shared_ptr<Util::ScrollBarController>    scrollBarController,
	QWidget*                                      parent
)
	: QDialog(parentWidgetProvider->GetWidget(parent))
	, m_impl(*this, *modelProvider, std::move(settings), std::move(itemViewToolTipper), std::move(scrollBarController))
{
}

SettingsDialog::~SettingsDialog() = default;
