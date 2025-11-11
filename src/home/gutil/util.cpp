#include "util.h"

#include <QHeaderView>
#include <QMenu>
#include <QTreeView>

#include "interface/localization.h"

#include "util/ISettings.h"

namespace HomeCompa::Util
{

QRect GetGlobalGeometry(const QWidget& widget)
{
	const auto  geometry    = widget.geometry();
	const auto* parent      = widget.parentWidget();
	const auto  topLeft     = parent->mapToGlobal(geometry.topLeft());
	const auto  bottomRight = parent->mapToGlobal(geometry.bottomRight());
	QRect       rect { topLeft, bottomRight };
	return rect;
}

void SaveHeaderSectionWidth(const QHeaderView& header, ISettings& settings, const QString& key)
{
	QVector<int> widths;
	for (auto i = 0, sz = header.count() - 1; i < sz; ++i)
		widths << header.sectionSize(i);
	settings.Set(key, QVariant::fromValue(widths));
}

void LoadHeaderSectionWidth(QHeaderView& header, const ISettings& settings, const QString& key)
{
	if (const auto var = settings.Get(key, QVariant {}); var.isValid())
	{
		const auto widths = var.value<QVector<int>>();
		for (auto i = 0, sz = std::min(header.count() - 1, static_cast<int>(widths.size())); i < sz; ++i)
			header.resizeSection(i, widths[static_cast<qsizetype>(i)]);
	}
}

QMenu& FillTreeContextMenu(QTreeView& view, QMenu& menu)
{
	const auto has = [&](const bool value) {
		for (int row = 0, count = view.model()->rowCount(); row < count; ++row)
			if (view.isExpanded(view.model()->index(row, 0)) == value)
				return true;
		return false;
	};

	menu.addAction(
			Loc::Tr(Loc::CONTEXT_MENU, Loc::TREE_COLLAPSE_ALL),
			[&] {
				view.collapseAll();
			}
	)->setEnabled(has(true));
	menu.addAction(
			Loc::Tr(Loc::CONTEXT_MENU, Loc::TREE_EXPAND_ALL),
			[&] {
				view.expandAll();
			}
	)->setEnabled(has(false));

	return menu;
}

} // namespace HomeCompa::Util
