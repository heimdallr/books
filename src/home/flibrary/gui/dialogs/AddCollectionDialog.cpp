#include "ui_AddCollectionDialog.h"
#include "AddCollectionDialog.h"

#include <QStandardPaths>
#include <QStyle>

#include <plog/Log.h>

#include "GeometryRestorable.h"
#include "ParentWidgetProvider.h"

#include "interface/constants/Localization.h"
#include "interface/logic/ICollectionController.h"
#include "interface/ui/IUiFactory.h"

#include "zip.h"

#include "config/version.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto RECENT_TEMPLATE = "ui/AddCollectionDialog/%1";
constexpr auto NAME = "Name";
constexpr auto DATABASE = "DatabaseFileName";
constexpr auto FOLDER = "ArchiveFolder";
constexpr auto ADD_UN_INDEXED_BOOKS = "AddUnIndexedBooks";
constexpr auto SCAN_UN_INDEXED_FOLDERS = "ScanUnIndexedFolders";

constexpr auto CONTEXT                                    = "AddCollectionDialog";
constexpr auto DATABASE_FILENAME_FILTER = QT_TRANSLATE_NOOP("AddCollectionDialog", "Flibrary database files (*.db *.db3 *.s3db *.sl3 *.sqlite *.sqlite3 *.hlc *.hlc2);;All files (*.*)");
constexpr auto SELECT_DATABASE_FILE     = QT_TRANSLATE_NOOP("AddCollectionDialog", "Select database file");
constexpr auto SELECT_ARCHIVES_FOLDER   = QT_TRANSLATE_NOOP("AddCollectionDialog", "Select archives folder");

constexpr auto EMPTY_NAME                         = QT_TRANSLATE_NOOP("Error", "Name cannot be empty");
constexpr auto COLLECTION_NAME_ALREADY_EXISTS     = QT_TRANSLATE_NOOP("Error", "Same named collection has already been added");
constexpr auto EMPTY_DATABASE                     = QT_TRANSLATE_NOOP("Error", "Database file name cannot be empty");
constexpr auto COLLECTION_DATABASE_ALREADY_EXISTS = QT_TRANSLATE_NOOP("Error", "This collection has already been added: %1");
constexpr auto DATABASE_NOT_FOUND                 = QT_TRANSLATE_NOOP("Error", "Database file not found");
constexpr auto BAD_DATABASE_EXT                   = QT_TRANSLATE_NOOP("Error", "Bad database file extension (.inpx)");
constexpr auto EMPTY_ARCHIVES_NAME                = QT_TRANSLATE_NOOP("Error", "Archive folder name cannot be empty");
constexpr auto ARCHIVES_FOLDER_NOT_FOUND          = QT_TRANSLATE_NOOP("Error", "Archive folder not found");
constexpr auto EMPTY_ARCHIVES_FOLDER              = QT_TRANSLATE_NOOP("Error", "Archive folder cannot be empty");

constexpr auto DIALOG_KEY_DB = "Database";
constexpr auto DIALOG_KEY_ARCH = "Database";


QString Error(const char * str)
{
	if (!str)
		return {};

	return Loc::Tr(Loc::Ctx::ERROR, str);
}

TR_DEF

QString GetDatabase(const IUiFactory & uiController, const QString & file)
{
	return uiController.GetSaveFileName(DIALOG_KEY_DB, Tr(SELECT_DATABASE_FILE), Tr(DATABASE_FILENAME_FILTER), QFileInfo(file).path(), QFileDialog::DontConfirmOverwrite);
}

QString GetFolder(const IUiFactory & uiController, const QString & dir)
{
	return uiController.GetExistingDirectory(DIALOG_KEY_ARCH, Tr(SELECT_ARCHIVES_FOLDER), dir);
}

}

class AddCollectionDialog::Impl final
	: GeometryRestorable
	, GeometryRestorableObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	explicit Impl(AddCollectionDialog & self
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<ICollectionController> collectionController
		, std::shared_ptr<IUiFactory> uiFactory
	)
		: GeometryRestorable(*this, settings, "AddCollectionDialog")
		, GeometryRestorableObserver(self)
		, m_self(self)
		, m_settings(std::move(settings))
		, m_collectionController(std::move(collectionController))
		, m_uiFactory(std::move(uiFactory))
	{
		m_ui.setupUi(&m_self);

		connect(m_ui.editArchive, &QLineEdit::textChanged, &m_self, [&](const QString &text)
		{
			m_ui.btnSetDefaultName->setEnabled(!text.isEmpty());
			if (m_ui.editName->text().isEmpty())
				SetDefaultCollectionName();
		});

		m_ui.editName->setText(m_settings->Get(QString(RECENT_TEMPLATE).arg(NAME), QString("FLibrary")));
		m_ui.editDatabase->setText(m_settings->Get(QString(RECENT_TEMPLATE).arg(DATABASE), QString("%1/%2.db").arg(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation), PRODUCT_ID)));
		m_ui.editArchive->setText(m_settings->Get(QString(RECENT_TEMPLATE).arg(FOLDER)).toString());
		m_ui.checkBoxAddUnindexedBooks->setChecked(m_settings->Get(QString(RECENT_TEMPLATE).arg(ADD_UN_INDEXED_BOOKS), false));
		m_ui.checkBoxScanUnindexedArchives->setChecked(m_settings->Get(QString(RECENT_TEMPLATE).arg(SCAN_UN_INDEXED_FOLDERS), false));

		if (const auto inpx = m_uiFactory->GetNewCollectionInpx(); !inpx.empty())
			m_ui.editArchive->setText(QDir::fromNativeSeparators(QString::fromStdWString(inpx.parent_path())));

		connect(m_ui.btnSetDefaultName, &QAbstractButton::clicked, &m_self, [&] { SetDefaultCollectionName(true); });
		connect(m_ui.btnCreateNew, &QAbstractButton::clicked, &m_self, [&] { if (CheckData(Result::CreateNew)) m_self.done(Result::CreateNew); } );
		connect(m_ui.btnAdd, &QAbstractButton::clicked, &m_self, [&] { if (CheckData(Result::Add)) m_self.done(Result::Add); } );
		connect(m_ui.btnCancel, &QAbstractButton::clicked, &m_self, [&] { m_self.done(Result::Cancel); } );
		connect(m_ui.btnDatabase, &QAbstractButton::clicked, &m_self, [&]
		{
			if (const auto file = GetDatabase(*m_uiFactory, GetDatabaseFileName()); !file.isEmpty())
				m_ui.editDatabase->setText(file);
		});
		connect(m_ui.btnArchive, &QAbstractButton::clicked, &m_self, [&]
		{
			if (const auto dir = GetFolder(*m_uiFactory, GetArchiveFolder()); !dir.isEmpty())
				m_ui.editArchive->setText(dir);
		});

		connect(m_ui.editName, &QLineEdit::textChanged, &m_self, [&] { (void)CheckData(Result::Cancel); });
		connect(m_ui.editDatabase, &QLineEdit::textChanged, &m_self, [&] { (void)CheckData(Result::Cancel); });
		connect(m_ui.editArchive, &QLineEdit::textChanged, &m_self, [&] { (void)CheckData(Result::Cancel); });

		Init();
	}

	~Impl() override
	{
		if (m_self.result() == Result::Cancel)
			return;

		m_settings->Set(QString(RECENT_TEMPLATE).arg(NAME), GetName());
		m_settings->Set(QString(RECENT_TEMPLATE).arg(DATABASE), GetDatabaseFileName());
		m_settings->Set(QString(RECENT_TEMPLATE).arg(FOLDER), GetArchiveFolder());
		m_settings->Set(QString(RECENT_TEMPLATE).arg(ADD_UN_INDEXED_BOOKS), m_ui.checkBoxAddUnindexedBooks->isChecked());
		m_settings->Set(QString(RECENT_TEMPLATE).arg(SCAN_UN_INDEXED_FOLDERS), m_ui.checkBoxScanUnindexedArchives->isChecked());
	}

	QString GetName() const
	{
		return m_ui.editName->text().simplified();
	}

	QString GetDatabaseFileName() const
	{
		return m_ui.editDatabase->text();
	}

	QString GetArchiveFolder() const
	{
		return m_ui.editArchive->text();
	}

	bool AddUnIndexedBooks() const
	{
		return m_ui.checkBoxAddUnindexedBooks->isChecked();
	}

	bool ScanUnIndexedFolders() const
	{
		return m_ui.checkBoxScanUnindexedArchives->isChecked();
	}

private: // GeometryRestorableObserver
	void OnFontChanged(const QFont &) override
	{
		m_self.adjustSize();
		const auto height = m_self.sizeHint().height();
		if (height < 0)
			return;

		m_self.setMinimumHeight(height);
		m_self.setMaximumHeight(height);
	}

private:
	bool CheckData(const int mode) const
	{
		ResetError();

		return true
			&& CheckName()
			&& CheckDatabase(mode)
			&& CheckFolder(mode)
			;
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

	[[nodiscard]] bool CheckDatabase(const int mode) const
	{
		const auto db = GetDatabaseFileName();

		if (db.isEmpty())
			return SetErrorText(m_ui.editDatabase, Error(EMPTY_DATABASE));

		if (const auto name = m_collectionController->GetCollectionDatabaseName(db); !name.isEmpty())
			return SetErrorText(m_ui.editDatabase, Error(COLLECTION_DATABASE_ALREADY_EXISTS).arg(name).toStdString().data());

		if (mode == Result::Add && !QFile(db).exists())
			return SetErrorText(m_ui.editDatabase, Error(DATABASE_NOT_FOUND));

		if (mode == Result::CreateNew && QFileInfo(db).suffix().toLower() == "inpx")
			return SetErrorText(m_ui.editDatabase, Error(BAD_DATABASE_EXT));

		return true;
	}

	[[nodiscard]] bool CheckFolder(const bool showError = true) const
	{
		const auto folder = GetArchiveFolder();

		if (folder.isEmpty())
			return showError && SetErrorText(m_ui.editArchive, Error(EMPTY_ARCHIVES_NAME));

		if (!QDir(folder).exists())
			return showError && SetErrorText(m_ui.editArchive, Error(ARCHIVES_FOLDER_NOT_FOUND));

		if (QDir(folder).isEmpty())
			return showError && SetErrorText(m_ui.editArchive, Error(EMPTY_ARCHIVES_FOLDER));

		return true;
	}

	bool SetErrorText(QWidget * widget, const QString & text = {}) const
	{
		if (!text.isEmpty())
			PLOGW << text;

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

		if (!CheckFolder(buttonClicked))
			return false;

		const auto inpx = m_collectionController->GetInpx(GetArchiveFolder());
		assert(!inpx.isEmpty() && QFile::exists(inpx));

		try
		{
			m_ui.editName->setText(Zip(inpx).Read("collection.info").readLine().simplified());
		}
		catch(const std::exception & ex)
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
	}

private:
	AddCollectionDialog & m_self;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	PropagateConstPtr<ICollectionController, std::shared_ptr> m_collectionController;
	PropagateConstPtr<IUiFactory, std::shared_ptr> m_uiFactory;
	Ui::AddCollectionDialog m_ui {};
};

AddCollectionDialog::AddCollectionDialog(const std::shared_ptr<ParentWidgetProvider> & parentWidgetProvider
	, std::shared_ptr<ISettings> settings
	, std::shared_ptr<ICollectionController> collectionController
	, std::shared_ptr<IUiFactory> uiFactory
)
	: QDialog(parentWidgetProvider->GetWidget())
	, m_impl(*this
		, std::move(settings)
		, std::move(collectionController)
		, std::move(uiFactory)
	)
{
	PLOGD << "AddCollectionDialog created";
}

AddCollectionDialog::~AddCollectionDialog()
{
	PLOGD << "AddCollectionDialog destroyed";
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

bool AddCollectionDialog::AddUnIndexedBooks() const
{
	return m_impl->AddUnIndexedBooks();
}

bool AddCollectionDialog::ScanUnIndexedFolders() const
{
	return m_impl->ScanUnIndexedFolders();
}
