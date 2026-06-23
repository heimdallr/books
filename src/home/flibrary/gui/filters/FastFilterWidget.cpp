#include "ui_FastFilterWidget.h"

#include "FastFilterWidget.h"

#include <ranges>

#include <QJsonArray>
#include <QJsonDocument>
#include <QPushButton>
#include <QScreen>
#include <QTimer>

#include "fnd/FindPair.h"
#include "fnd/SignalBlocker.h"
#include "fnd/algorithm.h"

#include "interface/constants/ModelRole.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/localization.h"

#include "gutil/interface/IParentWidgetProvider.h"
#include "logic/data/DataItem.h"
#include "util/SortString.h"
#include "util/language.h"
#include "utilgui/GeometryRestorable.h"
#include "utilgui/ItemViewToolTipper.h"
#include "utilgui/ScrollBarController.h"

#include "QtTypes.h"
#include "log.h"

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

	static std::unique_ptr<const Translator> Create(const ISettings&)
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
	static std::unique_ptr<const Translator> Create(const ISettings&)
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
	static std::unique_ptr<const Translator> Create(const ISettings&)
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

class TranslatorRate final : public Translator
{
public:
	TranslatorRate(const ISettings& settings, const char* key, const int zeroSymbol)
		: m_symbol { settings.Get(key, Constant::Settings::STAR_SYMBOL_DEFAULT) }
		, m_zeroSymbol { zeroSymbol }
	{
	}

private: // ITranslator
	QString Translate(const Item& item) const override
	{
		if (item.id.toString().isEmpty())
			return {};

		const auto num = item.id.toInt();
		return num ? QString { num, m_symbol } : m_zeroSymbol != QChar { 0 } ? QString { m_zeroSymbol } : QString {};
	}

private:
	const QChar m_symbol, m_zeroSymbol;
};

namespace TranslatorLibRate
{

std::unique_ptr<const Translator> Create(const ISettings& settings)
{
	return std::make_unique<TranslatorRate>(settings, Constant::Settings::PREFER_LIB_RATE_STAR_SYMBOL_KEY, 0);
}

}

namespace TranslatorUserRate
{

std::unique_ptr<const Translator> Create(const ISettings& settings)
{
	return std::make_unique<TranslatorRate>(settings, Constant::Settings::PREFER_USER_RATE_STAR_SYMBOL_KEY, settings.Get(Constant::Settings::PREFER_USER_RATE_ZERO_SYMBOL_KEY, 0));
}

}

using TranslatorSeqNumber = TranslatorNumber;
using TranslatorYear      = TranslatorNumber;

constexpr std::pair<int, std::unique_ptr<const Translator> (*)(const ISettings&)> TRANSLATORS[] {
#define ITEM(NAME) {BookItem::Column::NAME, &Translator##NAME::Create}
	ITEM(Lang), ITEM(SeqNumber), ITEM(Year), ITEM(LibRate), ITEM(UserRate),
#undef ITEM
};

class Model final : public QAbstractListModel
{
public:
	Model(const QAbstractItemModel& model, const int column, const ISettings& settings, const QWidget* widget)
		: m_widget { widget }
		, m_translator { FindSecond(TRANSLATORS, column, &Translator::Create)(settings) }
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

			case ModelRole::SelectedItems:
			{
				const auto    values = value.value<QVariantList>() | std::ranges::to<std::unordered_set<QVariant, Util::VariantHash>>();
				std::set<int> changed;
				for (auto&& [item, n] : std::views::zip(m_items, std::views::iota(0)))
					if (item.checked != values.contains(item.id))
					{
						item.checked = !item.checked;
						changed.emplace(n);
					}

				for (const auto [begin, end] : Util::CreateRanges(changed))
					emit dataChanged(index(begin, 0), index(end - 1, 0), { Qt::CheckStateRole });

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
		std::shared_ptr<ISettings>                 settings,
		std::shared_ptr<Util::ItemViewToolTipper>  toolTipper,
		std::shared_ptr<Util::ScrollBarController> scrollBarController
	)
		: m_column { column }
		, m_callback { std::move(callback) }
		, m_settings { std::move(settings) }
		, m_toolTipper { std::move(toolTipper) }
		, m_scrollBarController { std::move(scrollBarController) }
		, m_model { std::unique_ptr<QAbstractItemModel> { std::make_unique<Model>(model, column, *m_settings, parentWidgetProvider.GetWidget()) } }
	{
		m_ui.setupUi(self);
		m_ui.view->setModel(m_model.get());
		m_ui.view->setFont(parentWidgetProvider.GetWidget()->font());
		m_ui.checkBoxAll->setCheckState(m_model->data({}, ModelRole::AllSelected).value<Qt::CheckState>());

		m_toolTipper->SetScrollArea(m_ui.view);
		m_scrollBarController->SetScrollArea(m_ui.view);

		const auto checkboxWidth = m_ui.view->style()->pixelMetric(QStyle::PM_IndicatorWidth);
		const auto contentWidth  = m_model->data({}, ModelRole::SizeRole).toInt() + checkboxWidth + 6 * 2;
		const auto toolbarWidth  = 3 * m_ui.buttonBox->button(QDialogButtonBox::Cancel)->width() + checkboxWidth * 2 + 6 * 6;

		const auto contentHeight = m_model->rowCount() * m_ui.view->rowHeight(0);
		const auto toolbarHeight = m_ui.btnRevertSelection->height() + 6 * 2 + 4;

		const auto screenSize = Util::GetActiveScreen(*parentWidgetProvider.GetWidget())->size();
		self->setFixedSize(std::min(std::max(contentWidth, toolbarWidth), screenSize.width() / 4), std::min(contentHeight + toolbarHeight, screenSize.height() - QCursor::pos().y() - 10));

		connect(m_ui.buttonBox, &QDialogButtonBox::clicked, this, &Impl::OnDialogButtonClicked);
		connect(m_ui.checkBoxAll, &QCheckBox::CHECK_STATE_CHANGED, this, [this](const CHECK_STATE checkState) {
			if (checkState == Qt::PartiallyChecked)
				return QTimer::singleShot(0, [this] {
					m_ui.checkBoxAll->setCheckState(Qt::Checked);
				});

			m_model->setData({}, checkState, Qt::CheckStateRole);
			m_ui.checkBoxAll->setToolTip(checkState == Qt::Checked ? tr("Deselect all") : tr("Select all"));
		});
		connect(m_ui.btnRevertSelection, &QAbstractButton::clicked, this, [this] {
			m_model->setData({}, Qt::PartiallyChecked, Qt::CheckStateRole);
		});
		connect(m_model.get(), &QAbstractItemModel::dataChanged, this, [this](const auto&, const auto&, const QVector<int>& roles) {
			if (roles.contains(Qt::CheckStateRole))
				SignalBlocker(m_ui.checkBoxAll)->setCheckState(m_model->data({}, ModelRole::AllSelected).value<Qt::CheckState>());
		});
		connect(m_ui.btnLoad, &QAbstractButton::clicked, this, &Impl::OnLoadClicked);
		connect(m_ui.btnSave, &QAbstractButton::clicked, this, &Impl::OnSaveClicked);

		m_ui.btnLoad->setEnabled(Deserialize(m_settings->Get(QString(Constant::Settings::FAST_FILTER_KEY_TEMPLATE).arg(m_column)).toByteArray(), true));
	}

private:
	void OnDialogButtonClicked(QAbstractButton* button) const
	{
		const auto role = m_ui.buttonBox->buttonRole(button);
		m_callback(role == QDialogButtonBox::ApplyRole, m_model->data({}, ModelRole::SelectedItems).value<QVariantList>());
	}

	void OnSaveClicked()
	{
		m_ui.btnLoad->setEnabled(true);
		m_settings->Set(QString(Constant::Settings::FAST_FILTER_KEY_TEMPLATE).arg(m_column), Serialize());
		PLOGI << "Filter values saved";
	}

	void OnLoadClicked()
	{
		if (const auto rangeVar = m_settings->Get(QString(Constant::Settings::FAST_FILTER_KEY_TEMPLATE).arg(m_column)); rangeVar.isValid())
		{
			if (Deserialize(rangeVar.toByteArray()))
			{
				PLOGI << "Filter values loaded";
				return;
			}
		}
		PLOGW << "Filter values load failed";
	}

	QByteArray Serialize() const
	{
		QJsonArray array;

		for (auto&& value : m_model->data({}, ModelRole::SelectedItems).value<QVariantList>())
			array.append(value.toJsonValue());

		return QJsonDocument(array).toJson(QJsonDocument::Compact);
	}

	bool Deserialize(const QByteArray& bytes, const bool checkOnly = false)
	{
		if (bytes.isEmpty())
			return false;

		QJsonParseError parseError;
		const auto      doc = QJsonDocument::fromJson(bytes, &parseError);
		if (parseError.error != QJsonParseError::NoError)
		{
			PLOGW << parseError.errorString();
			return false;
		}

		if (!doc.isArray())
		{
			PLOGW << "value must be an array";
			return false;
		}

		if (checkOnly)
			return true;

		const auto values = doc.array() | std::views::transform([](const auto& item) {
								return item.toVariant();
							})
		                  | std::ranges::to<QVariantList>();

		return m_model->setData({}, values, ModelRole::SelectedItems);
	}

private:
	const int                                     m_column;
	const Callback                                m_callback;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;

	PropagateConstPtr<Util::ItemViewToolTipper, std::shared_ptr>  m_toolTipper;
	PropagateConstPtr<Util::ScrollBarController, std::shared_ptr> m_scrollBarController;

	PropagateConstPtr<QAbstractItemModel> m_model;

	Ui::FastFilterWidget m_ui {};
};

FastFilterWidget::FastFilterWidget(
	const QAbstractItemModel&                  model,
	const int                                  column,
	Callback                                   callback,
	const IParentWidgetProvider&               parentWidgetProvider,
	std::shared_ptr<ISettings>                 settings,
	std::shared_ptr<Util::ItemViewToolTipper>  toolTipper,
	std::shared_ptr<Util::ScrollBarController> scrollBarController,
	QWidget*                                   parent
)
	: QWidget(parent)
	, m_impl(this, model, column, std::move(callback), parentWidgetProvider, std::move(settings), std::move(toolTipper), std::move(scrollBarController))
{
}

FastFilterWidget::~FastFilterWidget() = default;
