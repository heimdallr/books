#include "ui_AddCollectionDialog.h"

#include "AddCollectionDialog.h"

#include <QStandardPaths>
#include <QStyle>

#include "fnd/ScopedCall.h"

#include "interface/Localization.h"

#include "util/DyLib.h"
#include "util/GeometryRestorable.h"
#include "util/files.h"
#include "util/translit.h"

#include "Constant.h"
#include "log.h"
#include "zip.h"

#include "config/version.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto RECENT_TEMPLATE                  = "ui/AddCollectionDialog/%1";
constexpr auto NAME                             = "Name";
constexpr auto ADDITIONAL_FOLDER_ENABLED        = "AdditionalFolderEnabled";
constexpr auto INPX_EXPLICIT                    = "InpxExplicit";
constexpr auto ADD_UN_INDEXED_BOOKS             = "AddUnIndexedBooks";
constexpr auto SCAN_UN_INDEXED_FOLDERS          = "ScanUnIndexedFolders";
constexpr auto SKIP_NOT_IN_ARCHIVES             = "SkipNotInArchives";
constexpr auto LOAD_ANNOTATIONS                 = "LoadAnnotations";
constexpr auto MARK_UN_INDEXED_BOOKS_AS_DELETED = "MarkUnIndexedBooksAsDeleted";
constexpr auto DEFAULT_ARCHIVE_TYPE             = "DefaultArchiveType";
constexpr auto DEFAULT_ADDITIONAL_FOLDER        = "/etc";

constexpr auto CONTEXT               = "AddCollectionDialog";
constexpr auto CREATE_NEW_COLLECTION = QT_TRANSLATE_NOOP("AddCollectionDialog", "Create new");
constexpr auto ADD_COLLECTION        = QT_TRANSLATE_NOOP("AddCollectionDialog", "Add");

constexpr auto EMPTY_NAME                         = QT_TRANSLATE_NOOP("Error", "Name cannot be empty");
constexpr auto COLLECTION_NAME_ALREADY_EXISTS     = QT_TRANSLATE_NOOP("Error", "Same named collection has already been added");
constexpr auto EMPTY_DATABASE                     = QT_TRANSLATE_NOOP("Error", "You must specify the collection database file");
constexpr auto COLLECTION_DATABASE_ALREADY_EXISTS = QT_TRANSLATE_NOOP("Error", "This collection has already been added: %1");
constexpr auto DATABASE_NOT_FOUND                 = QT_TRANSLATE_NOOP("Error", "Database file not found");
constexpr auto BAD_DATABASE_EXT                   = QT_TRANSLATE_NOOP("Error", "Bad database file extension (.inpx)");
constexpr auto EMPTY_ARCHIVES_NAME                = QT_TRANSLATE_NOOP("Error", "Archive folder name cannot be empty");
constexpr auto ARCHIVES_FOLDER_NOT_FOUND          = QT_TRANSLATE_NOOP("Error", "Archive folder not found");
constexpr auto ADDITIONAL_FOLDER_NOT_FOUND        = QT_TRANSLATE_NOOP("Error", "Additional info folder not found");
constexpr auto EMPTY_ARCHIVES_FOLDER              = QT_TRANSLATE_NOOP("Error", "Archive folder cannot be empty");
constexpr auto INPX_NOT_FOUND                     = QT_TRANSLATE_NOOP("Error", "Index file (*.inpx) not found");
constexpr auto CANNOT_CREATE_FOLDER               = QT_TRANSLATE_NOOP("Error", "Cannot create database folder %1");

QString Error(const char* str)
{
	if (!str)
		return {};

	return Loc::Tr(Loc::Ctx::ERROR_CTX, str);
}

TR_DEF

} // namespace

class AddCollectionDialog::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(AddCollectionDialog& self, std::shared_ptr<ISettings> settings, std::shared_ptr<ICollectionController> collectionController, std::shared_ptr<const IUiFactory> uiFactory)
		: GeometryRestorable(*this, settings, CONTEXT)
		, GeometryRestorableObserver(self)
		, m_self { self }
		, m_settings { std::move(settings) }
		, m_collectionController { std::move(collectionController) }
		, m_uiFactory { std::move(uiFactory) }
		, m_icuLib { std::make_unique<Util::DyLib>(ICU::LIB_NAME) }
		, m_icuTransliterate { m_icuLib->GetTypedProc<ICU::TransliterateType>(ICU::TRANSLITERATE_NAME) }
	{
		m_ui.setupUi(&m_self);

		m_ui.comboBoxDefaultArchiveType->addItems(Zip::GetTypes());

		connect(m_ui.editArchive, &RelativePathLineEdit::Changed, &m_self, [&](const QString& text) {
			m_ui.btnSetDefaultName->setEnabled(!text.isEmpty());
			if (m_ui.editName->text().isEmpty())
				SetDefaultCollectionName();
		});

		if (const auto inpxFolder = m_uiFactory->GetNewCollectionInpxFolder(); !inpxFolder.empty())
			m_ui.editArchive->SetText(QDir::fromNativeSeparators(QString::fromStdWString(inpxFolder)));

		connect(m_ui.btnSetDefaultName, &QAbstractButton::clicked, &m_self, [&] {
			SetDefaultCollectionName(true);
		});
		connect(m_ui.btnAdd, &QAbstractButton::clicked, &m_self, [&] {
			if (m_createMode)
			{
				const auto db  = GetDatabaseFileName();
				const auto dir = QFileInfo(db).dir();
				if (!dir.exists() && !dir.mkpath("."))
				{
					SetErrorText(m_ui.editDatabase, Error(CANNOT_CREATE_FOLDER).arg(dir.path()));
					return;
				}
			}

			if (CheckData())
				m_self.done(m_createMode ? Result::CreateNew : Result::Add);
		});
		connect(m_ui.btnCancel, &QAbstractButton::clicked, &m_self, [&] {
			m_self.done(Result::Cancel);
		});

		connect(m_ui.editName, &QLineEdit::textChanged, &m_self, [&](const QString& text) {
			UpdateDatabasePath(text);
			(void)CheckData();
		});
		connect(m_ui.editDatabase, &RelativePathLineEdit::Edited, &m_self, [this] {
			m_userDefinedDatabasePath = true;
		});
		connect(m_ui.editDatabase, &RelativePathLineEdit::Changed, &m_self, [&](const QString& db) {
			OnDatabaseNameChanged(db);
		});
		connect(m_ui.editArchive, &RelativePathLineEdit::Changed, &m_self, [&](const QString& folder) {
			OnArchiveFolderPathChanged(folder);
		});
		connect(m_ui.editAdditional, &RelativePathLineEdit::Changed, &m_self, [&] {
			(void)CheckData();
		});
		connect(m_ui.checkBoxScanUnindexedArchives, &QCheckBox::checkStateChanged, &m_self, [&] {
			(void)CheckData();
		});
		connect(m_ui.editInpx, &RelativePathLineEdit::Changed, &m_self, [&] {
			(void)CheckData();
		});
		connect(m_ui.checkBoxInpx, &QCheckBox::toggled, &m_self, [&] {
			(void)CheckData();
		});

		connect(m_ui.checkBoxAdditional, &QCheckBox::toggled, [this](const bool checked) {
			if (checked && QDir(m_ui.editArchive->GetText() + DEFAULT_ADDITIONAL_FOLDER).exists())
				m_ui.editAdditional->SetText(m_ui.editArchive->GetText() + DEFAULT_ADDITIONAL_FOLDER);
			(void)CheckData();
		});

		m_ui.editName->setText(m_settings->Get(QString(RECENT_TEMPLATE).arg(NAME), QString("FLibrary")));
		m_ui.editDatabase->Setup(&m_self, m_settings.get(), m_uiFactory.get(), m_ui.btnDatabase, QString("%1/%2.db").arg(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation), PRODUCT_ID));
		m_ui.editInpx->Setup(&m_self, m_settings.get(), m_uiFactory.get(), m_ui.btnInpx);
		m_ui.editArchive->Setup(&m_self, m_settings.get(), m_uiFactory.get(), m_ui.btnArchive);
		m_ui.editAdditional->Setup(&m_self, m_settings.get(), m_uiFactory.get(), m_ui.btnAdditional);

		m_ui.checkBoxAddUnindexedBooks->setChecked(m_settings->Get(QString(RECENT_TEMPLATE).arg(ADD_UN_INDEXED_BOOKS), true));
		m_ui.checkBoxMarkUnindexedAdDeleted->setChecked(m_settings->Get(QString(RECENT_TEMPLATE).arg(MARK_UN_INDEXED_BOOKS_AS_DELETED), true));
		m_ui.checkBoxScanUnindexedArchives->setChecked(m_settings->Get(QString(RECENT_TEMPLATE).arg(SCAN_UN_INDEXED_FOLDERS), false));
		m_ui.checkBoxAddMissingBooks->setChecked(!m_settings->Get(QString(RECENT_TEMPLATE).arg(SKIP_NOT_IN_ARCHIVES), true));
		m_ui.checkBoxAdditional->setChecked(m_settings->Get(QString(RECENT_TEMPLATE).arg(ADDITIONAL_FOLDER_ENABLED), true));
		m_ui.checkBoxAnnotation->setChecked(m_settings->Get(QString(RECENT_TEMPLATE).arg(LOAD_ANNOTATIONS), false));
		m_ui.checkBoxInpx->setChecked(m_settings->Get(QString(RECENT_TEMPLATE).arg(INPX_EXPLICIT), false));

		m_ui.comboBoxDefaultArchiveType->setCurrentIndex([this] {
			const auto defaultArchiveType = m_settings->Get(QString(RECENT_TEMPLATE).arg(DEFAULT_ARCHIVE_TYPE), QString("7z"));
			auto       index              = m_ui.comboBoxDefaultArchiveType->findText(defaultArchiveType);
			if (index < 0)
			{
				m_ui.comboBoxDefaultArchiveType->addItem(defaultArchiveType);
				index = m_ui.comboBoxDefaultArchiveType->count() - 1;
			}
			return index;
		}());

		LoadGeometry();
	}

	~Impl() override
	{
		SaveGeometry();
		if (m_self.result() == Result::Cancel)
			return;

		m_settings->Set(QString(RECENT_TEMPLATE).arg(NAME), GetName());
		m_settings->Set(QString(RECENT_TEMPLATE).arg(ADD_UN_INDEXED_BOOKS), m_ui.checkBoxAddUnindexedBooks->isChecked());
		m_settings->Set(QString(RECENT_TEMPLATE).arg(MARK_UN_INDEXED_BOOKS_AS_DELETED), m_ui.checkBoxMarkUnindexedAdDeleted->isChecked());
		m_settings->Set(QString(RECENT_TEMPLATE).arg(SCAN_UN_INDEXED_FOLDERS), m_ui.checkBoxScanUnindexedArchives->isChecked());
		m_settings->Set(QString(RECENT_TEMPLATE).arg(SKIP_NOT_IN_ARCHIVES), !m_ui.checkBoxAddMissingBooks->isChecked());
		m_settings->Set(QString(RECENT_TEMPLATE).arg(ADDITIONAL_FOLDER_ENABLED), m_ui.checkBoxAdditional->isChecked());
		m_settings->Set(QString(RECENT_TEMPLATE).arg(LOAD_ANNOTATIONS), m_ui.checkBoxAnnotation->isChecked());
		m_settings->Set(QString(RECENT_TEMPLATE).arg(INPX_EXPLICIT), m_ui.checkBoxInpx->isChecked());
		m_settings->Set(QString(RECENT_TEMPLATE).arg(DEFAULT_ARCHIVE_TYPE), m_ui.comboBoxDefaultArchiveType->currentText());
	}

	QString GetName() const
	{
		return m_ui.editName->text().simplified();
	}

	QString GetDatabaseFileName() const
	{
		return m_ui.editDatabase->GetText();
	}

	QString GetArchiveFolder() const
	{
		return m_ui.editArchive->GetText();
	}

	QString GetAdditionalFolder() const
	{
		return m_ui.checkBoxAdditional->isChecked() ? m_ui.editAdditional->GetText() : QString {};
	}

	QString GetInpx() const
	{
		return m_ui.checkBoxInpx->isChecked() ? m_ui.editInpx->GetText() : QString {};
	}

	QString GetDefaultArchiveType() const
	{
		return m_ui.comboBoxDefaultArchiveType->currentText();
	}

	bool AddUnIndexedBooks() const
	{
		return m_ui.checkBoxAddUnindexedBooks->isChecked();
	}

	bool ScanUnIndexedFolders() const
	{
		return m_ui.checkBoxScanUnindexedArchives->isChecked();
	}

	bool SkipLostBooks() const
	{
		return !m_ui.checkBoxAddMissingBooks->isChecked();
	}

	bool MarkUnIndexedBooksAsDeleted() const
	{
		return m_ui.checkBoxMarkUnindexedAdDeleted->isChecked();
	}

	bool LoadAnnotations() const
	{
		return m_ui.checkBoxAdditional->isChecked() && m_ui.checkBoxAnnotation->isChecked();
	}

private: // GeometryRestorableObserver
	void OnFontChanged(const QFont&) override
	{
		m_self.adjustSize();
		const auto height = m_self.sizeHint().height();
		if (height < 0)
			return;

		m_self.setMinimumHeight(height);
		m_self.setMaximumHeight(height);
	}

private:
	void UpdateDatabasePath(QString text) const
	{
		if (m_userDefinedDatabasePath || text.isEmpty())
			return;

		const QFileInfo fileInfo(m_ui.editDatabase->GetText().isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) : m_ui.editDatabase->GetText());
		m_ui.editDatabase->SetText(QString("%1.db").arg(fileInfo.dir().filePath(Util::Transliterate(m_icuTransliterate, std::move(text)))));
	}

	void OnDatabaseNameChanged(const QString& db)
	{
		m_createMode = !db.isEmpty() && !QFile::exists(db);
		m_ui.btnAdd->setText(Tr(m_createMode ? CREATE_NEW_COLLECTION : ADD_COLLECTION));
		m_ui.options->setEnabled(m_createMode);
	}

	void OnArchiveFolderPathChanged(const QString& path) const
	{
		if (m_ui.checkBoxAdditional->isChecked() && m_ui.editAdditional->GetText().isEmpty() && QDir(path + DEFAULT_ADDITIONAL_FOLDER).exists())
			m_ui.editAdditional->SetText(path + DEFAULT_ADDITIONAL_FOLDER);
	}

	bool CheckData() const
	{
		ResetError();

		return CheckName() && CheckDatabase() && CheckFolder();
	}

	[[nodiscard]] bool CheckName() const
	{
		const auto name = GetName();

		if (name.isEmpty())
			return SetErrorText(m_ui.editName, Error(EMPTY_NAME));

		if (m_collectionController->IsCollectionNameExists(name))
			return SetErrorText(m_ui.editName, Error(COLLECTION_NAME_ALREADY_EXISTS));

		return true;
	}

	[[nodiscard]] bool CheckDatabase() const
	{
		const auto db = Util::ToAbsolutePath(GetDatabaseFileName());

		if (db.isEmpty())
			return SetErrorText(m_ui.editDatabase, Error(EMPTY_DATABASE));

		if (const auto name = m_collectionController->GetCollectionDatabaseName(db); !name.isEmpty())
			return SetErrorText(m_ui.editDatabase, Error(COLLECTION_DATABASE_ALREADY_EXISTS).arg(name).toStdString().data());

		if (!m_createMode && !QFile(db).exists())
			return SetErrorText(m_ui.editDatabase, Error(DATABASE_NOT_FOUND));

		if (m_createMode && QFileInfo(db).suffix().toLower() == "inpx")
			return SetErrorText(m_ui.editDatabase, Error(BAD_DATABASE_EXT));

		return true;
	}

	[[nodiscard]] bool CheckFolder() const
	{
		const auto folder = Util::ToAbsolutePath(GetArchiveFolder());

		if (folder.isEmpty())
			return SetErrorText(m_ui.editArchive, Error(EMPTY_ARCHIVES_NAME));

		if (!QDir(folder).exists())
			return SetErrorText(m_ui.editArchive, Error(ARCHIVES_FOLDER_NOT_FOUND));

		if (QDir(folder).isEmpty())
			return SetErrorText(m_ui.editArchive, Error(EMPTY_ARCHIVES_FOLDER));

		if (m_createMode && !m_ui.checkBoxScanUnindexedArchives->isChecked())
		{
			if (m_ui.checkBoxInpx->isChecked())
			{
				if (!QFile::exists(Util::ToAbsolutePath(m_ui.editInpx->GetText())))
					return SetErrorText(m_ui.editInpx, Error(INPX_NOT_FOUND));
			}
			else if (!m_collectionController->IsCollectionFolderHasInpx(folder))
				return SetErrorText(m_ui.editArchive, Error(INPX_NOT_FOUND));
		}

		if (m_ui.checkBoxAdditional->isChecked() && !QDir(m_ui.editAdditional->GetText()).exists())
			return SetErrorText(m_ui.editAdditional, Error(ADDITIONAL_FOLDER_NOT_FOUND));

		return true;
	}

	bool SetErrorText(QWidget* widget, const QString& text = {}) const
	{
		widget->setProperty("errorTag", text.isEmpty() ? "false" : "true");
		widget->style()->unpolish(widget);
		widget->style()->polish(widget);
		widget->setFont(m_self.font());

		m_ui.lblError->setText(text);
		return text.isEmpty();
	}

	bool SetDefaultCollectionName(const bool buttonClicked = false) const
	{
		if (buttonClicked)
			ResetError();

		if (!CheckFolder())
			return false;

		const auto inpxPath = [this]() -> QString {
			if (auto inpx = GetInpx(); !inpx.isEmpty())
				return inpx;
			if (auto inpx = m_collectionController->GetInpxFiles(GetArchiveFolder()); !inpx.empty())
				return *inpx.begin();
			return {};
		}();

		if (!QFile::exists(inpxPath))
			return true;

		try
		{
			Zip zip(inpxPath);
			if (zip.GetFileNameList().contains(QString::fromStdWString(Inpx::COLLECTION_INFO)))
				m_ui.editName->setText(zip.Read(QString::fromStdWString(Inpx::COLLECTION_INFO))->GetStream().readLine().simplified());
		}
		catch (const std::exception& ex)
		{
			return buttonClicked && SetErrorText(m_ui.editArchive, ex.what());
		}

		return true;
	}

	void ResetError() const
	{
		SetErrorText(m_ui.editName);
		SetErrorText(m_ui.editDatabase);
		SetErrorText(m_ui.editArchive);
		SetErrorText(m_ui.editAdditional);
		SetErrorText(m_ui.editInpx);
	}

private:
	AddCollectionDialog&                                      m_self;
	PropagateConstPtr<ISettings, std::shared_ptr>             m_settings;
	PropagateConstPtr<ICollectionController, std::shared_ptr> m_collectionController;
	std::shared_ptr<const IUiFactory>                         m_uiFactory;
	std::unique_ptr<Util::DyLib>                              m_icuLib;
	ICU::TransliterateType                                    m_icuTransliterate { nullptr };
	bool                                                      m_createMode { false };
	bool                                                      m_userDefinedDatabasePath { false };
	Ui::AddCollectionDialog                                   m_ui {};
};

AddCollectionDialog::AddCollectionDialog(
	const std::shared_ptr<IParentWidgetProvider>& parentWidgetProvider,
	std::shared_ptr<ISettings>                    settings,
	std::shared_ptr<ICollectionController>        collectionController,
	std::shared_ptr<const IUiFactory>             uiFactory
)
	: QDialog(parentWidgetProvider->GetWidget())
	, m_impl(*this, std::move(settings), std::move(collectionController), std::move(uiFactory))
{
	PLOGV << "AddCollectionDialog created";
}

AddCollectionDialog::~AddCollectionDialog()
{
	PLOGV << "AddCollectionDialog destroyed";
}

int AddCollectionDialog::Exec()
{
	return exec();
}

QString AddCollectionDialog::GetName() const
{
	return m_impl->GetName();
}

QString AddCollectionDialog::GetDatabaseFileName() const
{
	return m_impl->GetDatabaseFileName();
}

QString AddCollectionDialog::GetArchiveFolder() const
{
	return m_impl->GetArchiveFolder();
}

QString AddCollectionDialog::GetAdditionalFolder() const
{
	return m_impl->GetAdditionalFolder();
}

QString AddCollectionDialog::GetInpx() const
{
	return m_impl->GetInpx();
}

QString AddCollectionDialog::GetDefaultArchiveType() const
{
	return m_impl->GetDefaultArchiveType();
}

bool AddCollectionDialog::AddUnIndexedBooks() const
{
	return m_impl->AddUnIndexedBooks();
}

bool AddCollectionDialog::ScanUnIndexedFolders() const
{
	return m_impl->ScanUnIndexedFolders();
}

bool AddCollectionDialog::SkipLostBooks() const
{
	return m_impl->SkipLostBooks();
}

bool AddCollectionDialog::MarkUnIndexedBooksAsDeleted() const
{
	return m_impl->MarkUnIndexedBooksAsDeleted();
}

bool AddCollectionDialog::LoadAnnotations() const
{
	return m_impl->LoadAnnotations();
}
