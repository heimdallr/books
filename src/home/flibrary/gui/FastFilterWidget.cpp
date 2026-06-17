#include "ui_FastFilterWidget.h"

#include "FastFilterWidget.h"

#include <QPushButton>
#include <QScreen>

#include "fnd/algorithm.h"

#include "interface/constants/ModelRole.h"

#include "gutil/interface/IParentWidgetProvider.h"
#include "utilgui/GeometryRestorable.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace
{

struct ModelRole
{
	enum
	{
		SizeRole = Role::Last,
		Selected,
	};
};

class Model final : public QAbstractListModel
{
private:
	struct Item
	{
		QVariant id;
		QString  title;
		bool     checked { false };
	};

	using Items = std::vector<Item>;

public:
	Model(const QAbstractItemModel& model, const int column, const QWidget* widget)
		: m_widget { widget }
		, m_items { model.data({}, Role::AuthorsAll + column).value<QVariantList>() | std::views::as_rvalue
		            | std::views::transform([currentFilter = model.data({}, Role::AuthorFilter + column).value<const std::unordered_set<QVariant, Util::VariantHash>*>()](auto&& item) {
						  Item element { .id = std::move(item) };
						  element.title   = element.id.toString();
						  element.checked = currentFilter->contains(element.id);
						  return element;
					  })
		            | std::ranges::to<std::vector>() }
	{
		std::ranges::sort(m_items, {}, [](const auto& item) {
			return item.title;
		});
	}

private: // QAbstractItemModel
	int rowCount(const QModelIndex& parent) const override
	{
		return parent.isValid() ? 0 : static_cast<int>(m_items.size());
	}

	QVariant data(const QModelIndex& index, const int role) const override
	{
		if (!index.isValid())
		{
			switch (role)
			{
				case ModelRole::SizeRole:
					return GetWidth();

				case ModelRole::Selected:
					return m_items | std::views::filter([](const auto& item) {
							   return item.checked;
						   })
					     | std::views::transform([](const auto& item) {
							   return item.id;
						   })
					     | std::ranges::to<QVariantList>();

				default:
					assert(false && "unexpected role");
			}
		}
		assert(index.isValid() && index.row() < rowCount({}));
		const auto& item = m_items[index.row()];

		switch (role)
		{
			case Qt::DisplayRole:
				return item.title;

			case Qt::CheckStateRole:
				return item.checked ? Qt::Checked : Qt::Unchecked;

			case Qt::FontRole:
				return m_widget->font();

			default:
				break;
		}

		return {};
	}

	bool setData(const QModelIndex& index, const QVariant& value, const int role) override
	{
		if (!index.isValid())
		{
			switch (role)
			{
				case Qt::CheckStateRole:
				{
					const auto checked = value.value<Qt::CheckState>();
					const auto f       = checked == Qt::Checked ? std::function<void(Item&)>([](Item& item) {
						item.checked = true;
					})
					                                            : checked == Qt::Unchecked
					                       ? [](Item& item) {
										   item.checked = false;
											 } : [](Item& item) {
										   item.checked = !item.checked;
											 };

					for (auto& item : m_items)
						f(item);

					emit dataChanged(this->index(0, 0), this->index(rowCount({}) - 1), { Qt::CheckStateRole });
					return true;
				}

				default:
					break;
			}

			return assert(false && "unexpected role"), false;
		}
		assert(index.isValid() && index.row() < rowCount({}));
		auto& item = m_items[index.row()];

		switch (role)
		{
			case Qt::CheckStateRole:
				return Util::Set(item.checked, value.value<Qt::CheckState>() == Qt::Checked);

			default:
				assert(false && "unexpected role");
				break;
		}

		return {};
	}

	Qt::ItemFlags flags(const QModelIndex& index) const override
	{
		return QAbstractListModel::flags(index) | Qt::ItemIsUserCheckable;
	}

private:
	int GetWidth() const
	{
		QFontMetrics fontMetrics(m_widget->font());
		int          width = -1;
		for (const auto& item : m_items)
			width = std::max(width, fontMetrics.boundingRect(item.title).width());

		return width + 20;
	}

private:
	const QWidget* const m_widget;
	Items                m_items;
};

} // namespace

class FastFilterWidget::Impl final : public QObject
{
public:
	Impl(QWidget* self, const QAbstractItemModel& model, const int column, Callback callback, const IParentWidgetProvider& parentWidgetProvider)
		: m_self { self }
		, m_callback { std::move(callback) }
		, m_model { std::unique_ptr<QAbstractItemModel> { std::make_unique<Model>(model, column, self) } }
	{
		m_ui.setupUi(self);
		m_ui.view->setModel(m_model.get());

		const auto contentWidth = m_model->data({}, ModelRole::SizeRole).toInt() + m_self->style()->pixelMetric(QStyle::PM_IndicatorWidth);
		const auto toolbarWidth = 2 * m_ui.buttonBox->button(QDialogButtonBox::Ok)->width() + m_ui.btnSelectAll->height() * 3 + 6 * 6;

		const auto contentHeight = m_model->rowCount() * m_ui.view->rowHeight(0);
		const auto toolbarHeight = m_ui.btnSelectAll->height() + 6 * 2;

		const auto screenSize = Util::GetActiveScreen(*parentWidgetProvider.GetWidget())->size();
		m_self->setFixedSize(std::min(std::max(contentWidth, toolbarWidth), screenSize.width() / 5), std::min(contentHeight + toolbarHeight, screenSize.height() - QCursor::pos().y() - 10));

		connect(m_ui.buttonBox, &QDialogButtonBox::clicked, this, &Impl::OnDialogButtonClicked);
		connect(m_ui.btnSelectAll, &QAbstractButton::clicked, this, [this] {
			m_model->setData({}, Qt::Checked, Qt::CheckStateRole);
		});
		connect(m_ui.btnUnselectAll, &QAbstractButton::clicked, this, [this] {
			m_model->setData({}, Qt::Unchecked, Qt::CheckStateRole);
		});
		connect(m_ui.btnRevertSelection, &QAbstractButton::clicked, this, [this] {
			m_model->setData({}, Qt::PartiallyChecked, Qt::CheckStateRole);
		});
	}

private:
	void OnDialogButtonClicked(QAbstractButton* button) const
	{
		const auto role = m_ui.buttonBox->buttonRole(button);
		m_callback(role == QDialogButtonBox::AcceptRole, m_model->data({}, ModelRole::Selected).value<QVariantList>());
	}

private:
	QWidget*                              m_self;
	const Callback                        m_callback;
	PropagateConstPtr<QAbstractItemModel> m_model;

	Ui::FastFilterWidget m_ui {};
};

FastFilterWidget::FastFilterWidget(const QAbstractItemModel& model, const int column, Callback callback, const IParentWidgetProvider& parentWidgetProvider, QWidget* parent)
	: QWidget(parent)
	, m_impl(this, model, column, std::move(callback), parentWidgetProvider)
{
}

FastFilterWidget::~FastFilterWidget() = default;
