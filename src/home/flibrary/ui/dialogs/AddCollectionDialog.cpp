#include "ui_AddCollectionDialog.h"
#include "AddCollectionDialog.h"

#include <QFileDialog>
#include <QFileInfo>
#include <plog/Log.h>

#include "GeometryRestorable.h"
#include "ParentWidgetProvider.h"

#include "interface/constants/Localization.h"
#include "interface/logic/ICollectionController.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto RECENT_TEMPLATE = "ui/AddCollectionDialog/%1";
constexpr auto NAME = "Name";
constexpr auto DATABASE = "DatabaseFileName";
constexpr auto FOLDER = "ArchiveFolder";

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
constexpr auto INPX_NOT_FOUND                     = QT_TRANSLATE_NOOP("Error", "Index file (*.inpx) not found");

QString Error(const char * str)
{
    return Loc::Tr(Loc::Ctx::ERROR, str);
}

QString Tr(const char * str)
{
    return Loc::Tr(CONTEXT, str);
}


QString GetDatabase(QWidget & parent, const QString & file)
{
	return QFileDialog::getSaveFileName(&parent, Tr(SELECT_DATABASE_FILE), QFileInfo(file).path(), Tr(DATABASE_FILENAME_FILTER), nullptr, QFileDialog::DontConfirmOverwrite);
}

QString GetFolder(QWidget & parent, const QString & dir)
{
    return QFileDialog::getExistingDirectory(&parent, Tr(SELECT_ARCHIVES_FOLDER), dir);
}

}

class AddCollectionDialog::Impl final
    : GeometryRestorable
    , GeometryRestorable::IObserver
{
    NON_COPY_MOVABLE(Impl)

public:
    explicit Impl(AddCollectionDialog & self
        , std::shared_ptr<ISettings> settings
        , std::shared_ptr<ICollectionController> collectionController
    )
        : GeometryRestorable(*this, settings, "AddCollectionDialog")
		, m_self(self)
		, m_settings(std::move(settings))
		, m_collectionController(std::move(collectionController))
    {
        m_ui.setupUi(&m_self);

        m_ui.editName->setText(m_settings->Get(QString(RECENT_TEMPLATE).arg(NAME)).toString());
        m_ui.editDatabase->setText(m_settings->Get(QString(RECENT_TEMPLATE).arg(DATABASE)).toString());
        m_ui.editArchive->setText(m_settings->Get(QString(RECENT_TEMPLATE).arg(FOLDER)).toString());

        connect(m_ui.btnCreateNew, &QAbstractButton::clicked, &m_self, [&] { if (CheckData(Result::CreateNew)) m_self.done(Result::CreateNew); } );
        connect(m_ui.btnAdd, &QAbstractButton::clicked, &m_self, [&] { if (CheckData(Result::Add)) m_self.done(Result::Add); } );
        connect(m_ui.btnCancel, &QAbstractButton::clicked, &m_self, [&] { m_self.done(Result::Cancel); } );
        connect(m_ui.btnDatabase, &QAbstractButton::clicked, &m_self, [&]
        {
            if (const auto file = GetDatabase(m_self, GetDatabaseFileName()); !file.isEmpty())
				m_ui.editDatabase->setText(file);
        });
        connect(m_ui.btnArchive, &QAbstractButton::clicked, &m_self, [&]
        {
            if (const auto dir = GetFolder(m_self, GetArchiveFolder()); !dir.isEmpty())
                m_ui.editArchive->setText(dir);
        });

        connect(m_ui.editName, &QLineEdit::textChanged, &m_self, [&] { CheckData(Result::Cancel); });
        connect(m_ui.editDatabase, &QLineEdit::textChanged, &m_self, [&] { CheckData(Result::Cancel); });
        connect(m_ui.editArchive, &QLineEdit::textChanged, &m_self, [&] { CheckData(Result::Cancel); });

        GeometryRestorable::Init();
    }

    ~Impl() override
    {
        if (m_self.result() == Result::Cancel)
            return;

        m_settings->Set(QString(RECENT_TEMPLATE).arg(NAME), GetName());
        m_settings->Set(QString(RECENT_TEMPLATE).arg(DATABASE), GetDatabaseFileName());
        m_settings->Set(QString(RECENT_TEMPLATE).arg(FOLDER), GetArchiveFolder());
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

private: // GeometryRestorable::IObserver
    QWidget & GetWidget() noexcept override
    {
        return m_self;
    }

    void OnFontSizeChanged(int /*fontSize*/) override
    {
        m_self.adjustSize();
        const auto height = m_self.sizeHint().height();
        m_self.setMinimumHeight(height);
        m_self.setMaximumHeight(height);
    }

private:
    bool CheckData(const int mode) const
    {
        SetErrorText(m_ui.editName);
        SetErrorText(m_ui.editDatabase);
        SetErrorText(m_ui.editArchive);

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
            return SetErrorText(m_ui.editDatabase, Error(COLLECTION_DATABASE_ALREADY_EXISTS).arg(name));

		if (mode == Result::Add && !QFile(db).exists())
			return SetErrorText(m_ui.editDatabase, Error(DATABASE_NOT_FOUND));

		if (mode == Result::CreateNew && QFileInfo(db).suffix().toLower() == "inpx")
			return SetErrorText(m_ui.editDatabase, Error(BAD_DATABASE_EXT));

        return true;
    }

    [[nodiscard]] bool CheckFolder(const int mode) const
    {
        const auto folder = GetArchiveFolder();

        if (folder.isEmpty())
            return SetErrorText(m_ui.editArchive, Error(EMPTY_ARCHIVES_NAME));

		if (!QDir(folder).exists())
			return SetErrorText(m_ui.editArchive, Error(ARCHIVES_FOLDER_NOT_FOUND));

		if (QDir(folder).isEmpty())
			return SetErrorText(m_ui.editArchive, Error(EMPTY_ARCHIVES_FOLDER));

		if (mode == Result::CreateNew && !m_collectionController->IsCollectionFolderHasInpx(folder))
			return SetErrorText(m_ui.editArchive, Error(INPX_NOT_FOUND));

        return true;
    }

    bool SetErrorText(QWidget * widget, const QString & text = {}) const
    {
        widget->setStyleSheet(QString("border: %1").arg(text.isEmpty() ? "1px solid black" : "2px solid red"));
        return text.isEmpty()
            ? (m_ui.lblError->setText(""), true)
            : (m_ui.lblError->setText(text), false)
            ;
    }

private:
    AddCollectionDialog & m_self;
    PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
    PropagateConstPtr<ICollectionController, std::shared_ptr> m_collectionController;
    Ui::AddCollectionDialog m_ui {};
};

AddCollectionDialog::AddCollectionDialog(const std::shared_ptr<ParentWidgetProvider> & parentWidgetProvider
    , std::shared_ptr<ISettings> settings
    , std::shared_ptr<ICollectionController> collectionController
)
    : QDialog(parentWidgetProvider->GetWidget())
	, m_impl(*this, std::move(settings), std::move(collectionController))
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
