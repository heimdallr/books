#pragma once

#include <QGroupBox>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

namespace Ui
{
class ImageSettingsWidget;
}

namespace HomeCompa
{
class ISettings;
}

namespace HomeCompa::fb2cut
{

struct ImageSettings;

class ImageSettingsWidget final : public QGroupBox
{
	Q_OBJECT
	NON_COPY_MOVABLE(ImageSettingsWidget)

signals:
	void Changed() const;

public:
	explicit ImageSettingsWidget(std::shared_ptr<ISettings> settingsManager, QWidget* parent = nullptr);
	~ImageSettingsWidget() override;

public:
	void SetCommonSettings(const ImageSettingsWidget& commonImageSettingsWidget);
	void SetImageSettings(ImageSettings& settings);
	void ApplySettings();

private:
	void CopyFromCommon();

private:
	PropagateConstPtr<Ui::ImageSettingsWidget> m_ui;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settingsManager;
	const ImageSettingsWidget* m_commonImageSettingsWidget { nullptr };
	ImageSettings* m_settings { nullptr };
};

} // namespace HomeCompa::fb2cut
