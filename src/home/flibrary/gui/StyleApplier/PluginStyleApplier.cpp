#include "PluginStyleApplier.h"

#include <QApplication>
#include <QPainterStateGuard>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOptionMenuItem>
#include <QWindow>

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

class MenuSvgFixStyle final : public QProxyStyle
{
public:
	explicit MenuSvgFixStyle(const QString& style)
		: QProxyStyle(QStyleFactory::create(style))
	{
	}

private: // QProxyStyle
	void drawControl(const ControlElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget = nullptr) const override
	{
		if (element == CE_MenuBarItem)
		{
			if (const auto* menuOpt = qstyleoption_cast<const QStyleOptionMenuItem*>(option))
			{
				if (menuOpt->icon.isNull())
					return QProxyStyle::drawControl(element, option, painter, widget);

				QStyleOptionMenuItem textAndBgOpt = *menuOpt;
				QIcon                originalIcon = menuOpt->icon;

				textAndBgOpt.icon = QIcon();
				textAndBgOpt.text.clear();
				QProxyStyle::drawControl(element, &textAndBgOpt, painter, widget);

				QPainterStateGuard psg(painter);

				int iconSize = menuOpt->maxIconWidth;
				if (iconSize <= 0 && widget)
				{
					iconSize = widget->style()->pixelMetric(PM_SmallIconSize, option, widget);
					if (iconSize <= 0)
						iconSize = 16;
				}

				const auto xPosition = menuOpt->rect.left() + (menuOpt->rect.width() - iconSize) / 2;
				const auto yPosition = menuOpt->rect.top() + (menuOpt->rect.height() - iconSize) / 2;

				const QRect iconRect(xPosition, yPosition, iconSize, iconSize);

				const auto dpr = widget && widget->windowHandle() ? widget->windowHandle()->devicePixelRatio() : painter && painter->device() ? painter->device()->devicePixelRatioF() : 1.0;

				const auto pixmap = originalIcon.pixmap(QSize(iconSize, iconSize), dpr, QIcon::Normal);
				painter->drawPixmap(iconRect, pixmap);

				return;
			}
		}

		QProxyStyle::drawControl(element, option, painter, widget);
	}
};

PluginStyleApplier::PluginStyleApplier(std::shared_ptr<ISettings> settings)
	: AbstractThemeApplier(std::move(settings))
{
	PLOGV << "PluginStyleApplier created";
}

PluginStyleApplier::~PluginStyleApplier()
{
	PLOGV << "PluginStyleApplier destroyed";
}

IStyleApplier::Type PluginStyleApplier::GetType() const noexcept
{
	return Type::PluginStyle;
}

std::unique_ptr<Platform::DyLib> PluginStyleApplier::Set(QApplication& app) const
{
	auto style = m_settings->Get(THEME_NAME_KEY, THEME_NAME_DEFAULT);
	if (!QStyleFactory::keys().contains(style, Qt::CaseInsensitive))
		style = THEME_NAME_DEFAULT;

	if (style == "Fusion")
		QApplication::setStyle(new MenuSvgFixStyle(style));
	else
		QApplication::setStyle(style);

	app.setStyleSheet(ReadStyleSheet(STYLE_FILE_NAME));

	return {};
}
