#include "ui_AddCollectionDialog.h"
#include "AddCollectionDialog.h"

#include <QFileDialog>
#include <QFileInfo>

#include "GeometryRestorable.h"
#include "ParentWidgetProvider.h"

#include "interface/logic/ICollectionController.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto RECENT_TEMPLATE = "ui/AddCollectionDialog/%1";
constexpr auto NAME = "Name";
constexpr auto DATABASE = "DatabaseFileName";
constexpr auto FOLDER = "ArchiveFolder";

constexpr auto DATABASE_FILENAME_FILTER = "Flibrary database files (*.db *.db3 *.s3db *.sl3 *.sqlite *.sqlite3 *.hlc *.hlc2);;All files (*.*)";
//constexpr auto ERROR_TEXT_TEMPLATE = R"(<html><head/><body><p><span style="color:#ff0000;">%1</span></p></body></html>)";

QString GetDatabase(QWidget & parent, const QString & file)
{
	return QFileDialog::getSaveFileName(&parent, QWidget::tr("Select database file"), QFileInfo(file).path(), QWidget::tr(DATABASE_FILENAME_FILTER), nullptr, QFileDialog::DontConfirmOverwrite);
}

QString GetFolder(QWidget & parent, const QString & dir)
{
    return QFileDialog::getExistingDirectory(&parent, QWidget::tr("Select archives folder"), dir);
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
            return SetErrorText(m_ui.editName, tr("Name cannot be empty"));

        if (m_collectionController->IsCollectionNameExists(name))
            return SetErrorText(m_ui.editName, tr("Same named collection has already been added"));

        return true;
    }

    [[nodiscard]] bool CheckDatabase(const int mode) const
    {
        const auto db = GetDatabaseFileName();

        if (db.isEmpty())
            return SetErrorText(m_ui.editDatabase, tr("Database file name cannot be empty"));

        if (const auto name = m_collectionController->GetCollectionDatabaseName(db); !name.isEmpty())
            return SetErrorText(m_ui.editDatabase, tr("This collection has already been added: %1").arg(name));

		if (mode == Result::Add && !QFile(db).exists())
			return SetErrorText(m_ui.editDatabase, tr("Database file not found"));

		if (mode == Result::CreateNew && QFileInfo(db).suffix().toLower() == "inpx")
			return SetErrorText(m_ui.editDatabase, tr("Bad database file extension (.inpx)"));

        return true;
    }

    [[nodiscard]] bool CheckFolder(const int mode) const
    {
        const auto folder = GetArchiveFolder();

        if (folder.isEmpty())
            return SetErrorText(m_ui.editArchive, tr("Archive folder name cannot be empty"));

		if (!QDir(folder).exists())
			return SetErrorText(m_ui.editArchive, tr("Archive folder not found"));

		if (QDir(folder).isEmpty())
			return SetErrorText(m_ui.editArchive, tr("Archive folder cannot be empty"));

		if (mode == Result::CreateNew && !m_collectionController->IsCollectionFolderHasInpx(folder))
			return SetErrorText(m_ui.editArchive, tr("Index file (*.inpx) not found"));

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
    Ui::AddCollectionDialogClass m_ui {};
};

AddCollectionDialog::AddCollectionDialog(const std::shared_ptr<class ParentWidgetProvider> & parentWidgetProvider
    , std::shared_ptr<ISettings> settings
    , std::shared_ptr<ICollectionController> collectionController
)
    : QDialog(parentWidgetProvider->GetWidget())
	, m_impl(*this, std::move(settings), std::move(collectionController))
{
}

AddCollectionDialog::~AddCollectionDialog() = default;

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
