#include "MainWindow.h"

#include "fnd/FindPair.h"

#include "GuiUtil/interface/IUiFactory.h"
#include "util/ISettings.h"
#include "util/files.h"

#include "ImageSettingsWidget.h"
#include "settings.h"
#include "ui_MainWindow.h"

using namespace HomeCompa::fb2cut;

namespace
{

constexpr auto KEY_INPUT_FILES = "ui/fb2cut/inputFiles";
constexpr auto KEY_DST_FOLDER = "ui/fb2cut/outputFolder";
constexpr auto KEY_EXT_ARCHIVER = "ui/fb2cut/externalArchiver";
constexpr auto KEY_EXT_ARCHIVER_CMD_LINE = "ui/fb2cut/externalArchiverCmdLine";
constexpr auto KEY_ARCHIVE_FORMAT = "ui/fb2cut/format";
constexpr auto KEY_FFMPEG = "ui/fb2cut/externalFfmpeg";
constexpr auto KEY_SAVE_FB2 = "ui/fb2cut/saveFb2";
constexpr auto KEY_ARCHIVE_FB2 = "ui/fb2cut/archiveFb2";
constexpr auto KEY_MIN_IMAGE_FILE_SIZE = "ui/fb2cut/minImageFileSize";
constexpr auto KEY_MAX_THREAD_COUNT = "ui/fb2cut/maxThreadCount";

}

MainWindow::MainWindow(std::shared_ptr<ISettings> settingsManager,
                       std::shared_ptr<const Util::IUiFactory> uiFactory,
                       std::shared_ptr<ImageSettingsWidget> common,
                       std::shared_ptr<ImageSettingsWidget> covers,
                       std::shared_ptr<ImageSettingsWidget> images,
                       QWidget* parent)
	: QMainWindow(parent)
	, GeometryRestorable(*this, settingsManager, "fb2cut/MainWindow")
	, GeometryRestorableObserver(static_cast<QWidget&>(*this))
	, m_settingsManager { std::move(settingsManager) }
	, m_uiFactory { std::move(uiFactory) }
	, m_common { std::move(common) }
	, m_covers { std::move(covers) }
	, m_images { std::move(images) }
{
	m_ui->setupUi(this);

	Init();

	m_common->setCheckable(false);

	m_covers->SetCommonSettings(*m_common);
	m_images->SetCommonSettings(*m_common);

	m_ui->common->layout()->addWidget(m_common.get());
	m_ui->covers->layout()->addWidget(m_covers.get());
	m_ui->images->layout()->addWidget(m_images.get());

	m_common->setTitle(m_ui->common->accessibleName());
	m_covers->setTitle(m_ui->covers->accessibleName());
	m_images->setTitle(m_ui->images->accessibleName());

	m_ui->editInputFiles->addAction(m_ui->actionInputFilesNotFound, QLineEdit::TrailingPosition);
	m_ui->editDstFolder->addAction(m_ui->actionDstFolderNotExists, QLineEdit::TrailingPosition);
	m_ui->editFfmpeg->addAction(m_ui->actionFfmpegNotFound, QLineEdit::TrailingPosition);

	connect(m_ui->btnStart, &QAbstractButton::clicked, this, &MainWindow::OnStartClicked);
	connect(m_ui->btnInputFiles, &QAbstractButton::clicked, this, &MainWindow::OnInputFilesClicked);
	connect(m_ui->btnDstFolder, &QAbstractButton::clicked, this, &MainWindow::OnDstFolderClicked);
	connect(m_ui->btnExternalArchiver, &QAbstractButton::clicked, this, &MainWindow::OnExternalArchiverClicked);
	connect(m_ui->btnDefaultArgs, &QAbstractButton::clicked, this, &MainWindow::OnExternalArchiverDefaultArgsClicked);
	connect(m_ui->btnFfmpeg, &QAbstractButton::clicked, this, &MainWindow::OnFfmpegClicked);

	connect(m_ui->editInputFiles, &QLineEdit::textChanged, this, &MainWindow::OnInputFilesChanged);
	connect(m_ui->editDstFolder, &QLineEdit::textChanged, this, &MainWindow::OnDstFolderChanged);
	connect(m_ui->editExternalArchiver, &QLineEdit::textChanged, this, &MainWindow::OnExternalArchiverChanged);
	connect(m_ui->editFfmpeg, &QLineEdit::textChanged, this, &MainWindow::OnFfmpegChanged);
	connect(m_ui->saveFb2, &QAbstractButton::toggled, this, [this](const bool checked) { m_ui->format->setEnabled(checked && m_ui->archiveFb2->isChecked()); });

	const Settings defaultSettings;

	m_ui->editInputFiles->setText(m_settingsManager->Get(KEY_INPUT_FILES, QString {}));
	m_ui->editDstFolder->setText(m_settingsManager->Get(KEY_DST_FOLDER, QString {}));
	m_ui->editExternalArchiver->setText(m_settingsManager->Get(KEY_EXT_ARCHIVER, defaultSettings.archiver));
	m_ui->editArchiverCommandLine->setText(m_settingsManager->Get(KEY_EXT_ARCHIVER_CMD_LINE, QString {}));
	m_ui->editFfmpeg->setText(m_settingsManager->Get(KEY_FFMPEG, defaultSettings.ffmpeg));
	m_ui->saveFb2->setChecked(m_settingsManager->Get(KEY_SAVE_FB2, defaultSettings.saveFb2));
	m_ui->archiveFb2->setChecked(m_settingsManager->Get(KEY_ARCHIVE_FB2, defaultSettings.archiveFb2));
	m_ui->minImageFileSize->setValue(m_settingsManager->Get(KEY_MIN_IMAGE_FILE_SIZE, defaultSettings.minImageFileSize));
	m_ui->maxThreadCount->setValue(m_settingsManager->Get(KEY_MAX_THREAD_COUNT, defaultSettings.maxThreadCount));
	if (const auto index = m_ui->format->findText(m_settingsManager->Get(KEY_ARCHIVE_FORMAT, Zip::FormatToString(defaultSettings.format))))
		m_ui->format->setCurrentIndex(index);
}

MainWindow::~MainWindow() = default;

void MainWindow::SetSettings(Settings* settings)
{
	m_settings = settings;

	const Settings defaultSettings;

	if (!m_settings->inputWildcards.isEmpty())
		m_ui->editInputFiles->setText(m_settings->inputWildcards.front());

	if (m_settings->dstDir != defaultSettings.dstDir)
		m_ui->editDstFolder->setText(m_settings->dstDir.path());

	if (m_settings->archiver != defaultSettings.archiver)
		m_ui->editExternalArchiver->setText(m_settings->archiver);

	if (m_settings->archiverOptions != defaultSettings.archiverOptions)
		m_ui->editArchiverCommandLine->setText(m_settings->archiverOptions);

	if (m_settings->format != defaultSettings.format)
		if (const auto index = m_ui->format->findText(Zip::FormatToString(m_settings->format)))
			m_ui->format->setCurrentIndex(index);

	if (m_settings->ffmpeg != defaultSettings.ffmpeg)
		m_ui->editFfmpeg->setText(m_settings->ffmpeg);

	if (m_settings->saveFb2 != defaultSettings.saveFb2)
		m_ui->saveFb2->setChecked(m_settings->saveFb2);

	if (m_settings->archiveFb2 != defaultSettings.archiveFb2)
		m_ui->archiveFb2->setChecked(m_settings->archiveFb2);

	if (m_settings->minImageFileSize != defaultSettings.minImageFileSize)
		m_ui->minImageFileSize->setValue(m_settings->minImageFileSize);

	if (m_settings->maxThreadCount != defaultSettings.maxThreadCount)
		m_ui->maxThreadCount->setValue(m_settings->maxThreadCount);

	CheckEnabled();

	m_covers->SetImageSettings(m_settings->cover);
	m_images->SetImageSettings(m_settings->image);

	m_covers->ApplySettings();
	m_images->ApplySettings();
	if (m_settings->cover != m_settings->image)
		m_covers->setChecked(true);

	m_common->SetImageSettings(m_settings->image);
}

void MainWindow::CheckEnabled()
{
	const bool startDisabled = false || m_ui->actionInputFilesNotFound->isVisible() || m_ui->actionDstFolderNotExists->isVisible() || m_ui->actionFfmpegNotFound->isVisible();

	m_ui->btnStart->setEnabled(!startDisabled);
}

void MainWindow::OnStartClicked()
{
	m_covers->ApplySettings();
	m_images->ApplySettings();

	m_settings->inputWildcards = { m_ui->editInputFiles->text() };
	m_settings->dstDir = m_ui->editDstFolder->text();
	m_settings->ffmpeg = m_ui->editFfmpeg->text();
	m_settings->format = Zip::FormatFromString(m_ui->format->currentText());
	m_settings->archiver = m_ui->editExternalArchiver->text();
	if (!m_ui->editArchiverCommandLine->text().isEmpty())
		m_settings->archiverOptions = m_ui->editArchiverCommandLine->text();

	m_settings->saveFb2 = m_ui->saveFb2->isChecked();
	m_settings->archiveFb2 = m_ui->archiveFb2->isChecked();

	m_settings->minImageFileSize = m_ui->minImageFileSize->value();
	m_settings->maxThreadCount = m_ui->maxThreadCount->value();

	m_settingsManager->Set(KEY_INPUT_FILES, m_ui->editInputFiles->text());
	m_settingsManager->Set(KEY_DST_FOLDER, m_ui->editDstFolder->text());
	m_settingsManager->Set(KEY_ARCHIVE_FORMAT, m_ui->format->currentText());
	m_settingsManager->Set(KEY_EXT_ARCHIVER, m_ui->editExternalArchiver->text());
	m_settingsManager->Set(KEY_EXT_ARCHIVER_CMD_LINE, m_ui->editArchiverCommandLine->text());
	m_settingsManager->Set(KEY_FFMPEG, m_ui->editFfmpeg->text());
	m_settingsManager->Set(KEY_SAVE_FB2, m_ui->saveFb2->isChecked());
	m_settingsManager->Set(KEY_ARCHIVE_FB2, m_ui->archiveFb2->isChecked());
	m_settingsManager->Set(KEY_MIN_IMAGE_FILE_SIZE, m_ui->minImageFileSize->value());
	m_settingsManager->Set(KEY_MAX_THREAD_COUNT, m_ui->maxThreadCount->value());

	QCoreApplication::exit(1);
}

void MainWindow::OnInputFilesClicked()
{
	if (const auto fileName = m_uiFactory->GetOpenFileName("fb2cut/InputArchives", tr("Select input archive")); !fileName.isEmpty())
		m_ui->editInputFiles->setText(fileName);
}

void MainWindow::OnDstFolderClicked()
{
	if (const auto folderName = m_uiFactory->GetExistingDirectory("fb2cut/OutputFolder", tr("Select output folder")); !folderName.isEmpty())
		m_ui->editDstFolder->setText(folderName);
}

void MainWindow::OnExternalArchiverClicked()
{
	if (const auto fileName = GetExecutableFileName("fb2cut/ExternalArchiver", tr("Select external archiver executable")); !fileName.isEmpty())
		m_ui->editExternalArchiver->setText(fileName);
}

void MainWindow::OnExternalArchiverDefaultArgsClicked()
{
	if (m_ui->editExternalArchiver->text().contains("7z"))
		Set7zDefaultSettings();
}

void MainWindow::OnFfmpegClicked()
{
	if (const auto fileName = GetExecutableFileName("fb2cut/ffmpeg", tr("Select ffmpeg executable")); !fileName.isEmpty())
		m_ui->editFfmpeg->setText(fileName);
}

void MainWindow::OnInputFilesChanged()
{
	const auto fileList = Util::ResolveWildcard(m_ui->editInputFiles->text());
	m_ui->labelInputFilesFound->setText(m_ui->labelInputFilesFound->accessibleName().arg(fileList.size()));
	m_ui->actionInputFilesNotFound->setVisible(fileList.isEmpty());
	CheckEnabled();
}

void MainWindow::OnDstFolderChanged()
{
	m_ui->actionDstFolderNotExists->setVisible(!QDir(m_ui->editDstFolder->text()).exists());
	CheckEnabled();
}

void MainWindow::OnExternalArchiverChanged()
{
	const auto text = m_ui->editExternalArchiver->text();
	m_ui->archiveOptionsFrame->setVisible(!text.isEmpty());
	m_ui->btnDefaultArgs->setEnabled(text.contains("7z"));
	CheckEnabled();
}

void MainWindow::OnFfmpegChanged()
{
	m_ui->actionFfmpegNotFound->setVisible(!(m_ui->editFfmpeg->text().isEmpty() || QFile(m_ui->editFfmpeg->text()).exists()));
	CheckEnabled();
}

QString MainWindow::GetExecutableFileName(const QString& key, const QString& title) const
{
	return m_uiFactory->GetOpenFileName(key, title, tr("Executables (*.exe);;All files (*.*)"));
}

void MainWindow::Set7zDefaultSettings()
{
	static constexpr std::pair<Zip::Format, const char*> args[] {
		{ Zip::Format::SevenZip, "a %dst%.7z -mx9 -sdel -m0=ppmd -ms=off -bt -mmt%1 %src%" },
		{      Zip::Format::Zip,                     "a %dst%.zip -mx9 -sdel -mmt%1 %src%" },
	};

	m_ui->editArchiverCommandLine->setText(QString(FindSecond(args, Zip::FormatFromString(m_ui->format->currentText()))).arg(m_ui->maxThreadCount->value()));
}
