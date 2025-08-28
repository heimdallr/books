#pragma once

#include <QMainWindow>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "gutil/GeometryRestorable.h"

namespace Ui
{
class MainWindowClass;
};

namespace HomeCompa
{
class ISettings;
}

namespace HomeCompa::Util
{
class IUiFactory;
}

namespace HomeCompa::fb2cut
{

struct Settings;
class ImageSettingsWidget;

class MainWindow final
	: public QMainWindow
	, Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	Q_OBJECT
	NON_COPY_MOVABLE(MainWindow)

public:
	MainWindow(std::shared_ptr<ISettings> settingsManager,
	           std::shared_ptr<const Util::IUiFactory> uiFactory,
	           std::shared_ptr<ImageSettingsWidget> common,
	           std::shared_ptr<ImageSettingsWidget> covers,
	           std::shared_ptr<ImageSettingsWidget> images,
	           QWidget* parent = nullptr);
	~MainWindow() override;

public:
	void SetSettings(Settings* settings);

private:
	void CheckEnabled();
	void OnStartClicked();
	void OnInputFilesClicked();
	void OnDstFolderClicked();
	void OnExternalArchiverClicked();
	void OnExternalArchiverDefaultArgsClicked();
	void OnFfmpegClicked();

	void OnInputFilesChanged();
	void OnDstFolderChanged();
	void OnExternalArchiverChanged();
	void OnFfmpegChanged();

	QString GetExecutableFileName(const QString& key, const QString& title) const;
	void Set7zDefaultSettings();

private:
	PropagateConstPtr<Ui::MainWindowClass> m_ui;
	Settings* m_settings { nullptr };
	PropagateConstPtr<ISettings, std::shared_ptr> m_settingsManager;
	std::shared_ptr<const Util::IUiFactory> m_uiFactory;
	PropagateConstPtr<ImageSettingsWidget, std::shared_ptr> m_common;
	PropagateConstPtr<ImageSettingsWidget, std::shared_ptr> m_covers;
	PropagateConstPtr<ImageSettingsWidget, std::shared_ptr> m_images;
};

} // namespace HomeCompa::fb2cut
