#include "ImageSettingsWidget.h"

#include "util/ISettings.h"

#include "settings.h"
#include "ui_ImageSettingsWidget.h"

using namespace HomeCompa::fb2cut;

namespace
{

constexpr ImageSettings DEFAULT_SETTINGS;
constexpr auto QUALITY = "quality";
constexpr auto WIDTH = "width";
constexpr auto HEIGHT = "height";
constexpr auto GRAYSCALE = "grayscale";
constexpr auto SKIP = "skip";
constexpr auto LOCK_SIZE = "lockSize";

QString GetKey(const QString& section, const QString& key)
{
	return QString("ui/fb2cut/%1/%2").arg(section, key);
}

}

ImageSettingsWidget::ImageSettingsWidget(std::shared_ptr<ISettings> settingsManager, QWidget* parent)
	: QGroupBox(parent)
	, m_settingsManager(std::move(settingsManager))
{
	m_ui->setupUi(this);

	connect(m_ui->btnFixSize,
	        &QAbstractButton::toggled,
	        this,
	        [this](const bool checked)
	        {
				m_ui->height->setEnabled(!checked);
				if (checked)
					m_ui->height->setValue(m_ui->width->value());
			});

	connect(m_ui->width,
	        &QSpinBox::valueChanged,
	        this,
	        [this](const int value)
	        {
				if (m_ui->btnFixSize->isChecked())
					m_ui->height->setValue(value);
			});

	connect(m_ui->quality, &QSpinBox::valueChanged, this, &ImageSettingsWidget::Changed);
	connect(m_ui->width, &QSpinBox::valueChanged, this, &ImageSettingsWidget::Changed);
	connect(m_ui->height, &QSpinBox::valueChanged, this, &ImageSettingsWidget::Changed);
	connect(m_ui->grayscale, &QCheckBox::toggled, this, &ImageSettingsWidget::Changed);
	connect(m_ui->skip, &QCheckBox::toggled, this, &ImageSettingsWidget::Changed);
}

ImageSettingsWidget::~ImageSettingsWidget() = default;

void ImageSettingsWidget::SetCommonSettings(const ImageSettingsWidget& commonImageSettingsWidget)
{
	m_commonImageSettingsWidget = &commonImageSettingsWidget;
	connect(m_commonImageSettingsWidget, &ImageSettingsWidget::Changed, this, &ImageSettingsWidget::CopyFromCommon);
	connect(this, &QGroupBox::toggled, this, &ImageSettingsWidget::CopyFromCommon);
}

void ImageSettingsWidget::CopyFromCommon()
{
	if (isChecked())
		return;

	assert(m_commonImageSettingsWidget);
	m_ui->quality->setValue(m_commonImageSettingsWidget->m_ui->quality->value());
	m_ui->width->setValue(m_commonImageSettingsWidget->m_ui->width->value());
	m_ui->height->setValue(m_commonImageSettingsWidget->m_ui->height->value());
	m_ui->grayscale->setChecked(m_commonImageSettingsWidget->m_ui->grayscale->isChecked());
	m_ui->skip->setChecked(m_commonImageSettingsWidget->m_ui->skip->isChecked());
}

void ImageSettingsWidget::SetImageSettings(ImageSettings& settings)
{
	m_settings = &settings;

	m_ui->btnFixSize->setChecked(m_settingsManager->Get(GetKey(m_settings->type, LOCK_SIZE), m_ui->btnFixSize->isChecked()));
	m_ui->quality->setValue(m_settingsManager->Get(GetKey(m_settings->type, QUALITY), DEFAULT_SETTINGS.quality));
	m_ui->height->setValue(m_settingsManager->Get(GetKey(m_settings->type, HEIGHT), DEFAULT_SETTINGS.maxSize.height()));
	m_ui->width->setValue(m_settingsManager->Get(GetKey(m_settings->type, WIDTH), DEFAULT_SETTINGS.maxSize.width()));
	m_ui->grayscale->setChecked(m_settingsManager->Get(GetKey(m_settings->type, GRAYSCALE), DEFAULT_SETTINGS.grayscale));
	m_ui->skip->setChecked(m_settingsManager->Get(GetKey(m_settings->type, SKIP), !DEFAULT_SETTINGS.save));

	if (m_settings->maxSize != DEFAULT_SETTINGS.maxSize)
	{
		m_ui->width->setValue(m_settings->maxSize.width());
		m_ui->height->setValue(m_settings->maxSize.height());
	}

	if (m_settings->quality != DEFAULT_SETTINGS.quality)
		m_ui->quality->setValue(m_settings->quality);

	if (m_settings->save != DEFAULT_SETTINGS.save)
		m_ui->skip->setChecked(!m_settings->save);

	if (m_settings->grayscale != DEFAULT_SETTINGS.grayscale)
		m_ui->grayscale->setChecked(m_settings->grayscale);
}

void ImageSettingsWidget::ApplySettings()
{
	assert(m_settings);
	m_settings->maxSize = { m_ui->width->value(), m_ui->height->value() };
	m_settings->quality = m_ui->quality->value();
	m_settings->save = !m_ui->skip->isChecked();
	m_settings->grayscale = m_ui->grayscale->isChecked();

	m_settingsManager->Set(GetKey(m_settings->type, LOCK_SIZE), m_ui->btnFixSize->isChecked());
	m_settingsManager->Set(GetKey(m_settings->type, QUALITY), m_ui->quality->value());
	m_settingsManager->Set(GetKey(m_settings->type, HEIGHT), m_ui->height->value());
	m_settingsManager->Set(GetKey(m_settings->type, WIDTH), m_ui->width->value());
	m_settingsManager->Set(GetKey(m_settings->type, GRAYSCALE), m_ui->grayscale->isChecked());
	m_settingsManager->Set(GetKey(m_settings->type, SKIP), m_ui->skip->isChecked());
}
