#pragma once

#include <QMainWindow>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

namespace Ui { class MainWindowClass; };

namespace HomeCompa::fb2cut {

struct Settings;

class MainWindow final : public QMainWindow
{
	Q_OBJECT
	NON_COPY_MOVABLE(MainWindow)

public:
	explicit MainWindow(Settings & settings, QWidget * parent = nullptr);
	~MainWindow() override;

private:
	void SetSettings();
	void CheckEnabled();
	void OnInputFilesChanged();
	void OnDstFolderChanged();

private:
	PropagateConstPtr<Ui::MainWindowClass> m_ui;
	Settings & m_settings;
};

}
