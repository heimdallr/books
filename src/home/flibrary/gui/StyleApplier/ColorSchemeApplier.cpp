#include "ColorSchemeApplier.h"

#include <QApplication>
#include <QGuiApplication>
#include <QPalette>
#include <QPixmapCache>
#include <QStyleHints>

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto BASE_STYLE_SHEET_PROPERTY = "flibraryBaseStyleSheet";

struct ThemeColors
{
	QColor background;
	QColor panel;
	QColor panel2;
	QColor text;
	QColor text2;
	QColor text3;
	QColor accent;
	QColor rating;
	QColor line;
	QColor lineStrong;
	QColor danger;
};

const ThemeColors& GetColors(const bool dark)
{
	static const ThemeColors light {
		QColor { "#e9edf3" }, QColor { "#ffffff" }, QColor { "#f4f7fb" }, QColor { "#18212f" }, QColor { "#55606f" }, QColor { "#8b95a4" },
		QColor { "#2f6fc0" }, QColor { "#c6941a" }, QColor { "#d2d7df" }, QColor { "#b9c1cc" }, QColor { "#c33a32" },
	};
	static const ThemeColors darkTheme {
		QColor { "#12151a" }, QColor { "#171b22" }, QColor { "#1d222b" }, QColor { "#e8ecf2" }, QColor { "#a6afbc" }, QColor { "#6c7580" },
		QColor { "#5b93df" }, QColor { "#e0b23f" }, QColor { "#2b3038" }, QColor { "#414955" }, QColor { "#e46a61" },
	};
	return dark ? darkTheme : light;
}

QPalette CreatePalette(const ThemeColors& colors)
{
	QPalette palette;
	palette.setColor(QPalette::Window, colors.background);
	palette.setColor(QPalette::WindowText, colors.text);
	palette.setColor(QPalette::Base, colors.panel);
	palette.setColor(QPalette::AlternateBase, colors.panel2);
	palette.setColor(QPalette::ToolTipBase, colors.panel);
	palette.setColor(QPalette::ToolTipText, colors.text);
	palette.setColor(QPalette::Text, colors.text);
	palette.setColor(QPalette::Button, colors.panel2);
	palette.setColor(QPalette::ButtonText, colors.text);
	palette.setColor(QPalette::BrightText, colors.danger);
	palette.setColor(QPalette::Light, colors.panel);
	palette.setColor(QPalette::Midlight, colors.panel2);
	palette.setColor(QPalette::Mid, colors.line);
	palette.setColor(QPalette::Dark, colors.lineStrong);
	palette.setColor(QPalette::Shadow, colors.background.darker(150));
	palette.setColor(QPalette::Highlight, colors.accent);
	palette.setColor(QPalette::HighlightedText, Qt::white);
	palette.setColor(QPalette::Link, colors.accent);
	palette.setColor(QPalette::LinkVisited, colors.accent);
	palette.setColor(QPalette::PlaceholderText, colors.text3);

	palette.setColor(QPalette::Disabled, QPalette::WindowText, colors.text3);
	palette.setColor(QPalette::Disabled, QPalette::Text, colors.text3);
	palette.setColor(QPalette::Disabled, QPalette::ButtonText, colors.text3);
	palette.setColor(QPalette::Disabled, QPalette::Highlight, colors.line);
	palette.setColor(QPalette::Disabled, QPalette::HighlightedText, colors.text2);
	return palette;
}

QString CreateThemeStyleSheet(const ThemeColors& colors)
{
	const auto accent = QStringLiteral("%1, %2, %3").arg(colors.accent.red()).arg(colors.accent.green()).arg(colors.accent.blue());
	const auto text   = QStringLiteral("%1, %2, %3").arg(colors.text.red()).arg(colors.text.green()).arg(colors.text.blue());
	return QString::fromLatin1(R"(

QTreeView::item:hover, QTableView::item:hover, QListView::item:hover {
	background-color: rgba(%1, 0.06);
}
QTreeView::item:selected, QTableView::item:selected, QListView::item:selected {
	background-color: rgba(%2, 0.18);
	color: palette(text);
}
QLabel[secondaryText="true"] { color: %3; }
QLabel[tertiaryText="true"] { color: %4; }
QLabel[rating="true"] { color: %5; }
QWidget[panel="true"] { background-color: %6; }
QWidget[panel2="true"] { background-color: %7; }
)")
	    .arg(text, accent, colors.text2.name(), colors.text3.name(), colors.rating.name(), colors.panel.name(), colors.panel2.name());
}

void LoadIconSet(const bool dark)
{
	static std::unique_ptr<Platform::DyLib> iconLibrary;
	static QString                          currentIconSet;

	const auto iconSet = dark ? QStringLiteral("icodark") : QStringLiteral("icolight");
	if (iconSet == currentIconSet)
		return;

	QPixmapCache::clear();
	iconLibrary.reset();
	iconLibrary    = std::make_unique<Platform::DyLib>(iconSet.toStdString());
	currentIconSet = iconSet;
}

void ApplyVisuals(QApplication& app, const Qt::ColorScheme colorScheme)
{
	const auto dark   = colorScheme == Qt::ColorScheme::Dark;
	const auto colors = GetColors(dark);
	app.setPalette(CreatePalette(colors));

	if (!app.property(BASE_STYLE_SHEET_PROPERTY).isValid())
		app.setProperty(BASE_STYLE_SHEET_PROPERTY, app.styleSheet());
	app.setStyleSheet(app.property(BASE_STYLE_SHEET_PROPERTY).toString() + CreateThemeStyleSheet(colors));
	LoadIconSet(dark);
}

void ConfigureColorScheme(QApplication& app, const QString& name)
{
	static QMetaObject::Connection systemColorSchemeConnection;
	QObject::disconnect(systemColorSchemeConnection);

	auto requested = Qt::ColorScheme::Unknown;
	if (name.compare(QStringLiteral("Light"), Qt::CaseInsensitive) == 0)
		requested = Qt::ColorScheme::Light;
	else if (name.compare(QStringLiteral("Dark"), Qt::CaseInsensitive) == 0)
		requested = Qt::ColorScheme::Dark;

	auto* styleHints = QGuiApplication::styleHints();
	styleHints->setColorScheme(requested);
	if (requested == Qt::ColorScheme::Unknown)
	{
		systemColorSchemeConnection = QObject::connect(styleHints, &QStyleHints::colorSchemeChanged, &app, [&app](const Qt::ColorScheme scheme) {
			ApplyVisuals(app, scheme);
		});
		requested                   = styleHints->colorScheme();
	}

	ApplyVisuals(app, requested);
}

} // namespace

ColorSchemeApplier::ColorSchemeApplier(std::shared_ptr<ISettings> settings)
	: AbstractStyleApplier(std::move(settings))
{
	PLOGV << "ColorSchemeApplier created";
}

ColorSchemeApplier::~ColorSchemeApplier()
{
	PLOGV << "ColorSchemeApplier destroyed";
}

IStyleApplier::Type ColorSchemeApplier::GetType() const noexcept
{
	return Type::ColorScheme;
}

void ColorSchemeApplier::Apply(const QString& name, const QString& /*data*/)
{
	m_settings->Set(COLOR_SCHEME_KEY, name);
	if (auto* app = qobject_cast<QApplication*>(QCoreApplication::instance()))
		ConfigureColorScheme(*app, name);
}

std::pair<QString, QString> ColorSchemeApplier::GetChecked() const
{
	return std::make_pair(m_settings->Get(COLOR_SCHEME_KEY, APP_COLOR_SCHEME_DEFAULT), QString {});
}

std::unique_ptr<Platform::DyLib> ColorSchemeApplier::Set(QApplication& app) const
{
	ConfigureColorScheme(app, m_settings->Get(COLOR_SCHEME_KEY, APP_COLOR_SCHEME_DEFAULT));
	return {};
}
