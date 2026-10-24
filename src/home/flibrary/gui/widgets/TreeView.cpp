#include "TreeView.h"

#include <QHeaderView>
#include <QPainter>

#include "fnd/ScopedCall.h"

#include "interface/constants/ModelRole.h"

#include "settings/ISettings.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace {

constexpr auto ESCALATE_UP                = "Preferences/ReadMark/escalateUpward";
constexpr auto READ_MARK_BACKGROUND_COLOR = "Preferences/ReadMark/backgroundColor%1";
constexpr auto READ_MARK_COLOR            = "Preferences/ReadMark/color%1";
constexpr auto READ_MARK_POSITION         = "Preferences/ReadMark/position%1";
constexpr auto READ_MARK_WIDTH            = "Preferences/ReadMark/width%1";

std::optional<QColor> GetReadMarkBackgroundColor(const ISettings& settings, const QString& suffix = {})
{
	if (const auto var = settings.Get(QString(READ_MARK_BACKGROUND_COLOR).arg(suffix)); var.isValid())
	{
		const auto value = var.toUInt();
		return QColor { static_cast<int>(value & 0xFF), static_cast<int>((value >> 8) & 0xFF), static_cast<int>((value >> 16) & 0xFF), 0xFF - static_cast<int>((value >> 24) & 0xFF) };
	}

	return {};
}

std::optional<QColor> GetReadMarkColor(const ISettings& settings, const QString& suffix = {})
{
	if (const auto var = settings.Get(QString(READ_MARK_COLOR).arg(suffix)); var.isValid())
	{
		const auto value = var.toUInt();
		return QColor { static_cast<int>(value & 0xFF), static_cast<int>((value >> 8) & 0xFF), static_cast<int>((value >> 16) & 0xFF), 0xFF - static_cast<int>((value >> 24) & 0xFF) };
	}

	return {};
}

std::optional<int> GetReadMarkWidth(const ISettings& settings, const QString& suffix = {})
{
	if (const auto var = settings.Get(QString(READ_MARK_WIDTH).arg(suffix)); var.isValid())
		return var.toInt();

	return {};
}

std::optional<int> GetReadMarkPosition(const ISettings& settings, const QString& suffix = {})
{
	if (const auto var = settings.Get(QString(READ_MARK_POSITION).arg(suffix)); var.isValid())
		return var.toInt();

	return {};
}

} // namespace

struct CustomTreeView::Impl
{
	bool isNavigation { false };

	std::shared_ptr<const ISettings> settings;

	std::optional<QColor> readMarkBackgroundColor;
	std::optional<QColor> readMarkColor;
	std::optional<int>    readMarkWidth;
	std::optional<int>    readMarkPosition;
	bool                  escalateUp { false };

	explicit Impl(QTreeView& view)
		: m_view { view }
	{
	}

	void Init(std::shared_ptr<const ISettings> settings_)
	{
		settings                = std::move(settings_);
		readMarkBackgroundColor = GetReadMarkBackgroundColor(*settings);
		readMarkColor           = GetReadMarkColor(*settings);
		readMarkWidth           = GetReadMarkWidth(*settings);
		readMarkPosition        = GetReadMarkPosition(*settings);
		escalateUp              = settings->Get(ESCALATE_UP, false);
	}

	void OnModelChanged(const QAbstractItemModel& model)
	{
		const QString suffix = model.data({}, Role::IsTree).toBool() ? TreeViewMode : ListViewMode;
		if (!((readMarkBackgroundColor = GetReadMarkBackgroundColor(*settings, suffix))))
			readMarkBackgroundColor = GetReadMarkBackgroundColor(*settings);

		if (!((readMarkColor = GetReadMarkColor(*settings, suffix))))
			readMarkColor = GetReadMarkColor(*settings);

		if (!((readMarkWidth = GetReadMarkWidth(*settings, suffix))))
			readMarkWidth = GetReadMarkWidth(*settings);

		if (!((readMarkPosition = GetReadMarkPosition(*settings, suffix))))
			readMarkPosition = GetReadMarkPosition(*settings);
	}

	void RenderReadMark(QPainter& painter, const QStyleOptionViewItem& options, const QModelIndex& index) const
	{
		if (isNavigation || index.data(Role::UserRate).toString().isEmpty() || !(index.data(Role::Type).value<ItemType>() == ItemType::Books || escalateUp))
			return;

		const ScopedCall painterGuard(
			[&] {
				painter.save();
			},
			[&] {
				painter.restore();
			}
		);

		if (readMarkBackgroundColor)
			painter.fillRect(options.rect, *readMarkBackgroundColor);

		const auto markColor = readMarkColor ? *readMarkColor : options.palette.color(QPalette::ColorRole::Text);

		auto o = options;

		if (index.data(Role::IsRemoved).toBool())
			o.palette.setColor(QPalette::ColorRole::Text, Qt::gray);

		if (readMarkWidth && m_view.header()->visualIndex(index.column()) == 0 && !index.data(Role::UserRate).toString().isEmpty())
		{
			QPen pen(markColor, *readMarkWidth);
			pen.setCapStyle(Qt::FlatCap);
			painter.setPen(pen);
			auto rect = o.rect;
			if (readMarkPosition)
				rect.setLeft(*readMarkPosition + *readMarkWidth / 2);
			painter.drawLine(rect.topLeft(), rect.bottomLeft());
			o.features &= ~QStyleOptionViewItem::Alternate;
		}
	}

private:
	QTreeView& m_view;
};

CustomTreeView::CustomTreeView(QWidget* parent)
	: QTreeView(parent)
	, m_impl(*this)
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
	m_impl->RenderReadMark(*painter, options, index);
	QTreeView::drawRow(painter, options, index);
}
