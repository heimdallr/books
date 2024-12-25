#include "ModeComboBox.h"

#include <ranges>

#include <QFile>
#include <QGuiApplication>
#include <QPainter>
#include <QSvgRenderer>
#include <QStyledItemDelegate>

#include "fnd/ScopedCall.h"
#include "util/ColorUtil.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto VALUE_MODE_ICON_TEMPLATE = ":/icons/%1.svg";

}

struct ModeComboBox::Impl : QStyledItemDelegate
{
    std::vector<std::unique_ptr<QSvgRenderer>> svgRenderers { CreateRenderers() };

private:
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::paint(painter, option, index);
        const auto adjust = option.rect.size() / 8;
        auto rect = option.rect.adjusted(adjust.width(), adjust.height(), -adjust.width(), -adjust.height());
        rect = rect.width() > rect.height()
            ? rect.adjusted((rect.width() - rect.height()) / 2, 0, -(rect.width() - rect.height()) / 2, 0)
            : rect.adjusted(0, (rect.height() - rect.width()) / 2, 0, -(rect.height() - rect.width()) / 2);
        svgRenderers[static_cast<size_t>(index.row())]->render(painter, rect);
    }

private:
    static std::vector< std::unique_ptr<QSvgRenderer>> CreateRenderers()
    {
        std::vector<std::unique_ptr<QSvgRenderer>> result;
        std::ranges::transform(VALUE_MODES | std::views::keys, std::back_inserter(result), [] (const auto & name)
        {
            auto renderer = std::make_unique<QSvgRenderer>();
            QFile file(QString(VALUE_MODE_ICON_TEMPLATE).arg(name));
            if (!file.open(QIODevice::ReadOnly))
                return renderer;

            const auto content = QString::fromUtf8(file.readAll()).arg(HomeCompa::Util::ToString(QGuiApplication::palette(), QPalette::Text));
            renderer->load(content.toUtf8());
            return renderer;
        });
        return result;
    }
};

ModeComboBox::ModeComboBox(QWidget *parent)
	: QComboBox(parent)
    , m_impl { std::make_unique<Impl>() }
{
	for (const auto * name : VALUE_MODES | std::views::keys)
		addItem("", QString(name));

    setItemDelegate(m_impl.get());
}

ModeComboBox::~ModeComboBox() = default;

void ModeComboBox::paintEvent(QPaintEvent * event)
{
    QComboBox::paintEvent(event);
    QPainter p;
    const ScopedCall painterGuard([&] { p.begin(this); }, [&] { p.end(); });
    
    QStyleOptionComboBox opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_PanelButtonBevel, &opt, &p, this);
    style()->drawPrimitive(QStyle::PE_PanelButtonCommand, &opt, &p, this);

    const auto adjust = opt.rect.size() / 8;
    m_impl->svgRenderers[static_cast<size_t>(currentIndex())]->render(&p, opt.rect.adjusted(adjust.width(), adjust.height(), -adjust.width(), -adjust.height()));
}
