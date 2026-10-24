#include "TreeView.h"

#include <QHeaderView>
#include <QPainter>

#include "interface/constants/ModelRole.h"

#include "settings/ISettings.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace {

constexpr auto READ_MARK_BACKGROUND_COLOR = "Preferences/ReadMark/backgroundColor%1";

std::optional<QColor> GetReadMarkBackgroundColor(const ISettings& settings, const QString& suffix = {})
{
	if (const auto var = settings.Get(QString(READ_MARK_BACKGROUND_COLOR).arg(suffix)); var.isValid())
	{
		const auto value = var.toUInt();
		return QColor { static_cast<int>(value & 0xFF), static_cast<int>((value >> 8) & 0xFF), static_cast<int>((value >> 16) & 0xFF), 0xFF - static_cast<int>((value >> 24) & 0xFF) };
	}

	return {};
}

}

struct CustomTreeView::Impl
{
	bool isNavigation { false };

	std::shared_ptr<const ISettings> settings;

	std::optional<QColor> readMarkBackgroundColor;

	void Init(std::shared_ptr<const ISettings> settings_)
	{
		settings                = std::move(settings_);
		readMarkBackgroundColor = GetReadMarkBackgroundColor(*settings);
	}

	void OnModelChanged(const QAbstractItemModel& model)
	{
		const QString suffix = model.data({}, Role::IsTree).toBool() ? TreeViewMode : ListViewMode;
		if (!((readMarkBackgroundColor = GetReadMarkBackgroundColor(*settings, suffix))))
			readMarkBackgroundColor = GetReadMarkBackgroundColor(*settings);
	}
};

CustomTreeView::CustomTreeView(QWidget* parent)
	: QTreeView(parent)
{
}

CustomTreeView::~CustomTreeView() = default;

void CustomTreeView::Init(std::shared_ptr<const ISettings> settings)
{
	m_impl->Init(std::move(settings));
}

void CustomTreeView::OnModelChanged(const QAbstractItemModel& model)
{
	m_impl->OnModelChanged(model);
}

void CustomTreeView::UpdateSectionSize()
{
	header()->setSectionResizeMode(1, QHeaderView::Interactive);
	if (const auto* model = this->model(); model && model->rowCount() > 0)
	{
		const auto height = rowHeight(model->index(0, 0));
		header()->resizeSection(1, height);
	}
}

void CustomTreeView::SetIsNavigation() noexcept
{
	m_impl->isNavigation = true;
}

void CustomTreeView::drawRow(QPainter* painter, const QStyleOptionViewItem& options, const QModelIndex& index) const
{
	const auto& impl = *m_impl;
	if (impl.isNavigation || !impl.readMarkBackgroundColor || index.data(Role::UserRate).toString().isEmpty())
		return QTreeView::drawRow(painter, options, index);

	painter->save();
	painter->fillRect(options.rect, *impl.readMarkBackgroundColor);
	painter->restore();

	QTreeView::drawRow(painter, options, index);
}
