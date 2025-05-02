#include "ItemViewToolTipper.h"

#include <QAbstractItemView>
#include <QHelpEvent>
#include <QToolTip>

#include "interface/constants/ProductConstant.h"

using namespace HomeCompa::Flibrary;

ItemViewToolTipper::ItemViewToolTipper(QObject* parent)
	: QObject(parent)
{
}

bool ItemViewToolTipper::eventFilter(QObject* obj, QEvent* event)
{
	if (event->type() != QEvent::ToolTip)
		return false;

	auto* view = qobject_cast<QAbstractItemView*>(obj->parent());
	if (!(view && view->model()))
		return false;

	const auto* helpEvent = static_cast<const QHelpEvent*>(event); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

	const auto pos = helpEvent->pos();
	const auto index = view->indexAt(pos);
	if (!index.isValid())
		return false;

	const auto& model = *view->model();
	const auto itemTooltip = model.data(index, Qt::ToolTipRole).toString();
	if (itemTooltip.isEmpty())
		return false;

	const auto itemText = model.data(index).toString();
	const auto authorAnnotationMode = itemText == Constant::INFO;

	auto font = view->font();
	font.setPointSizeF(font.pointSizeF() * (authorAnnotationMode ? 0.8 : 1.2));

	if (!authorAnnotationMode)
	{
		const int itemTextWidth = QFontMetrics(font).horizontalAdvance(itemText);

		const auto rect = view->visualRect(index);
		auto rectWidth = rect.width();

		if (model.flags(index) & Qt::ItemIsUserCheckable)
			rectWidth -= rect.height();

		if (itemTextWidth <= rectWidth)
			return true;
	}

	QToolTip::setFont(font);
	QToolTip::showText(helpEvent->globalPos(), itemTooltip, view);

	return true;
}
