#include "MainWindow.h"

#include <QDir>
#include <settings.h>

#include "util/files.h"

#include "ui_MainWindow.h"

using namespace HomeCompa::fb2cut;

MainWindow::MainWindow(Settings & settings, QWidget *parent)
	: QMainWindow(parent)
	, m_settings(settings)
{
	m_ui->setupUi(this);

	m_ui->editInputFiles->addAction(m_ui->actionInputFilesNotFound, QLineEdit::TrailingPosition);
	m_ui->editDstFolder->addAction(m_ui->actionDstFolderNotExists, QLineEdit::TrailingPosition);

	connect(m_ui->btnStart, &QAbstractButton::clicked, this, []{ QCoreApplication::exit(1); });
	connect(m_ui->editInputFiles, &QLineEdit::textChanged, this, &MainWindow::OnInputFilesChanged);
	connect(m_ui->editInputFiles, &QLineEdit::textChanged, this, &MainWindow::OnDstFolderChanged);

	SetSettings();
}

MainWindow::~MainWindow() = default;

void MainWindow::SetSettings()
{
	if (!m_settings.inputWildcards.isEmpty())
		m_ui->editInputFiles->setText(m_settings.inputWildcards.front());
	m_ui->editDstFolder->setText(m_settings.dstDir.path());
	CheckEnabled();
}

void MainWindow::CheckEnabled()
{
	const bool startDisabled = false
		|| m_ui->actionInputFilesNotFound->isVisible()
		|| m_ui->actionDstFolderNotExists->isVisible()
		;

	m_ui->btnStart->setEnabled(!startDisabled);
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
