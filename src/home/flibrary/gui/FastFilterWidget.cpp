#include "ui_FastFilterWidget.h"

#include "FastFilterWidget.h"

#include <QPushButton>
#include <QScreen>
#include <QTimer>

#include "fnd/FindPair.h"
#include "fnd/algorithm.h"

#include "interface/constants/ModelRole.h"
#include "interface/localization.h"

#include "gutil/interface/IParentWidgetProvider.h"
#include "logic/data/DataItem.h"
#include "util/SortString.h"
#include "util/language.h"
#include "utilgui/GeometryRestorable.h"
#include "utilgui/ItemViewToolTipper.h"
#include "utilgui/ScrollBarController.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace
{

struct ModelRole
{
	enum
	{
		SizeRole = Role::Last,
		SelectedItems,
		AllSelected,
	};
};

struct Item
{
	QVariant id;
	QString  title;
	bool     checked { false };
};

using Items = std::vector<Item>;

class Translator
{
	NON_COPY_MOVABLE(Translator)
public:
	Translator()          = default;
	virtual ~Translator() = default;

	static std::unique_ptr<const Translator> Create()
	{
		return std::make_unique<Translator>();
	}

public: // ITranslator
	virtual QString Translate(const Item& item) const
	{
		return item.id.toString();
	}

	virtual void Sort(Items& items) const
	{
		std::ranges::sort(items, [](const auto& lhs, const auto& rhs) {
			return Util::QStringWrapper::Compare(Util::QStringWrapper { lhs.title }, Util::QStringWrapper { rhs.title });
		});
	}
};

class TranslatorLang final : public Translator
{
public:
	static std::unique_ptr<const Translator> Create()
	{
		return std::make_unique<TranslatorLang>();
	}

private: // ITranslator
	QString Translate(const Item& item) const override
	{
		auto       language = Translator::Translate(item);
		const auto it       = m_languagesMap.find(language);
		return it != m_languagesMap.end() ? Loc::Tr(LANGUAGES_CONTEXT, it->second) : language;
	}

private:
	std::unordered_map<QString, const char*> m_languagesMap { GetLanguagesMap() };
};

class TranslatorNumber final : public Translator
{
public:
	static std::unique_ptr<const Translator> Create()
	{
		return std::make_unique<TranslatorNumber>();
	}

private: // ITranslator
	void Sort(Items& items) const override
	{
		std::ranges::sort(items, {}, [](const auto& item) {
			return item.title.isEmpty() ? std::numeric_limits<int>::max() : item.title.toInt();
		});
	}
};

using TranslatorSeqNumber = TranslatorNumber;
using TranslatorYear = TranslatorNumber;

constexpr std::pair<int, std::unique_ptr<const Translator> (*)()> TRANSLATORS[] {
#define ITEM(NAME) {BookItem::Column::NAME, &Translator##NAME::Create}
	ITEM(Lang),
	ITEM(SeqNumber),
	ITEM(Year),
#undef ITEM
};

class Model final : public QAbstractListModel
{
public:
	Model(const QAbstractItemModel& model, const int column, const QWidget* widget)
		: m_widget { widget }
		, m_translator { FindSecond(TRANSLATORS, column, &Translator::Create)() }
		, m_items { model.data({}, Role::AuthorsAll + column).value<QVariantList>() | std::views::as_rvalue
		            | std::views::transform([this, currentFilter = model.data({}, Role::AuthorFilter + column).value<const std::unordered_set<QVariant, Util::VariantHash>*>()](auto&& item) {
						  Item element { .id = std::move(item) };
						  element.title   = m_translator->Translate(element);
						  element.checked = currentFilter->contains(element.id);
						  return element;
					  })
		            | std::ranges::to<std::vector>() }
	{
		m_translator->Sort(m_items);
	}

private: // QAbstractItemModel
	int rowCount(const QModelIndex& parent) const override
	{
		return parent.isValid() ? 0 : static_cast<int>(m_items.size());
	}

	QVariant data(const QModelIndex& index, const int role) const override
	{
		return index.isValid() ? GetData(index, role) : GetData(role);
	}

	bool setData(const QModelIndex& index, const QVariant& value, const int role) override
	{
		return index.isValid() ? SetData(index, value, role) : SetData(value, role);
	}

	Qt::ItemFlags flags(const QModelIndex& index) const override
	{
		return QAbstractListModel::flags(index) | Qt::ItemIsUserCheckable;
	}

private:
	QVariant GetData(const QModelIndex& index, const int role) const
	{
		const auto& item = m_items[index.row()];

		switch (role)
		{
			case Qt::DisplayRole:
			case Qt::ToolTipRole:
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

	QVariant GetData(const int role) const
	{
		switch (role)
		{
			case ModelRole::SizeRole:
				return GetWidth();

			case ModelRole::SelectedItems:
				return m_items | std::views::filter([](const auto& item) {
						   return item.checked;
					   })
				     | std::views::transform([](const auto& item) {
						   return item.id;
					   })
				     | std::ranges::to<QVariantList>();

			case ModelRole::AllSelected:
			{
				const auto firstChecked = m_items.front().checked;
				return std::ranges::all_of(
						   m_items | std::views::drop(1),
						   [firstChecked](const auto& item) {
							   return item.checked == firstChecked;
						   }
					   )
				         ? (firstChecked ? Qt::Checked : Qt::Unchecked)
				         : Qt::PartiallyChecked;
			}

			default:
				assert(false && "unexpected role");
		}

		return {};
	}

	bool SetData(const QModelIndex& index, const QVariant& value, const int role)
	{
		auto& item = m_items[index.row()];

		switch (role)
		{
			case Qt::CheckStateRole:
				return Util::Set(item.checked, value.value<Qt::CheckState>() == Qt::Checked, [&] {
					emit dataChanged(index, index, { role });
				});

			default:
				assert(false && "unexpected role");
				break;
		}

		return {};
	}

	bool SetData(const QVariant& value, const int role)
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

	int GetWidth() const
	{
		QFontMetrics fontMetrics(m_widget->font());
		int          width = -1;
		for (const auto& item : m_items)
			width = std::max(width, fontMetrics.boundingRect(item.title).width());

		return width + 20;
	}

private:
	const QWidget* const              m_widget;
	std::unique_ptr<const Translator> m_translator;
	Items                             m_items;
};

} // namespace

class FastFilterWidget::Impl final : public QObject
{
public:
	Impl(
		QWidget*                                   self,
		const QAbstractItemModel&                  model,
		const int                                  column,
		Callback                                   callback,
		const IParentWidgetProvider&               parentWidgetProvider,
		std::shared_ptr<Util::ItemViewToolTipper>  toolTipper,
		std::shared_ptr<Util::ScrollBarController> scrollBarController
	)
		: m_self { self }
		, m_callback { std::move(callback) }
		, m_model { std::unique_ptr<QAbstractItemModel> { std::make_unique<Model>(model, column, parentWidgetProvider.GetWidget()) } }
		, m_toolTipper { std::move(toolTipper) }
		, m_scrollBarController { std::move(scrollBarController) }
	{
		m_ui.setupUi(self);
		m_ui.view->setModel(m_model.get());
		m_ui.view->setFont(parentWidgetProvider.GetWidget()->font());
		m_ui.checkBoxAll->setCheckState(m_model->data({}, ModelRole::AllSelected).value<Qt::CheckState>());

		m_toolTipper->SetScrollArea(m_ui.view);
		m_scrollBarController->SetScrollArea(m_ui.view);

		const auto checkboxWidth = m_self->style()->pixelMetric(QStyle::PM_IndicatorWidth);
		const auto contentWidth  = m_model->data({}, ModelRole::SizeRole).toInt() + checkboxWidth + 6 * 2;
		const auto toolbarWidth  = 2 * m_ui.buttonBox->button(QDialogButtonBox::Ok)->width() + checkboxWidth * 2 + 6 * 6;

		const auto contentHeight = m_model->rowCount() * m_ui.view->rowHeight(0);
		const auto toolbarHeight = m_ui.btnRevertSelection->height() + 6 * 2 + 4;

		const auto screenSize = Util::GetActiveScreen(*parentWidgetProvider.GetWidget())->size();
		m_self->setFixedSize(std::min(std::max(contentWidth, toolbarWidth), screenSize.width() / 4), std::min(contentHeight + toolbarHeight, screenSize.height() - QCursor::pos().y() - 10));

		connect(m_ui.buttonBox, &QDialogButtonBox::clicked, this, &Impl::OnDialogButtonClicked);
		connect(m_ui.checkBoxAll, &QCheckBox::checkStateChanged, this, [this](const Qt::CheckState checkState) {
			if (checkState == Qt::PartiallyChecked)
				return QTimer::singleShot(0, [this] {
					m_ui.checkBoxAll->setCheckState(Qt::Checked);
				});

			m_model->setData({}, checkState, Qt::CheckStateRole);
		});
		connect(m_ui.btnRevertSelection, &QAbstractButton::clicked, this, [this] {
			m_model->setData({}, Qt::PartiallyChecked, Qt::CheckStateRole);
		});
		connect(m_model.get(), &QAbstractItemModel::dataChanged, this, [this](const auto&, const auto&, const QList<int>& roles) {
			if (roles.contains(Qt::CheckStateRole))
			{
				const QSignalBlocker signalBlocked(m_ui.checkBoxAll);
				m_ui.checkBoxAll->setCheckState(m_model->data({}, ModelRole::AllSelected).value<Qt::CheckState>());
			}
		});
	}

private:
	void OnDialogButtonClicked(QAbstractButton* button) const
	{
		const auto role = m_ui.buttonBox->buttonRole(button);
		m_callback(role == QDialogButtonBox::AcceptRole, m_model->data({}, ModelRole::SelectedItems).value<QVariantList>());
	}

private:
	QWidget*                              m_self;
	const Callback                        m_callback;
	PropagateConstPtr<QAbstractItemModel> m_model;

	PropagateConstPtr<Util::ItemViewToolTipper, std::shared_ptr>  m_toolTipper;
	PropagateConstPtr<Util::ScrollBarController, std::shared_ptr> m_scrollBarController;

	Ui::FastFilterWidget m_ui {};
};

FastFilterWidget::FastFilterWidget(
	const QAbstractItemModel&                  model,
	const int                                  column,
	Callback                                   callback,
	const IParentWidgetProvider&               parentWidgetProvider,
	std::shared_ptr<Util::ItemViewToolTipper>  toolTipper,
	std::shared_ptr<Util::ScrollBarController> scrollBarController,
	QWidget*                                   parent
)
	: QWidget(parent)
	, m_impl(this, model, column, std::move(callback), parentWidgetProvider, std::move(toolTipper), std::move(scrollBarController))
{
}

FastFilterWidget::~FastFilterWidget() = default;
