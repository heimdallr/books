#include "util.h"

#include <ranges>

#include <QHeaderView>
#include <QMenu>
#include <QTreeView>

#include "interface/localization.h"

#include "settings/ISettings.h"

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
	QStringList widths;
	for (auto i = 0, sz = header.count() - 1; i < sz; ++i)
		widths << QString::number(header.sectionSize(i));
	settings.Set(key, widths);
}

void LoadHeaderSectionWidth(QHeaderView& header, const ISettings& settings, const QString& key)
{
	if (const auto var = settings.Get(key, QVariant {}); var.isValid())
		for (const auto widths = var.toStringList(); auto&& [index, width] : std::views::zip(std::views::iota(0, header.count() - 1), widths))
			header.resizeSection(index, width.toInt());
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
