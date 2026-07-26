#include "ui_DateIntervalFilterWidget.h"

#include "DateIntervalFilterWidget.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>

#include "interface/constants/ModelRole.h"
#include "interface/constants/SettingsConstant.h"

#include "gutil/interface/IParentWidgetProvider.h"
#include "settings/ISettings.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

namespace
{

constexpr auto MIN_KEY     = "min";
constexpr auto MAX_KEY     = "max";
constexpr auto DATE_FORMAT = "yyyy.MM.dd";

std::pair<QDate, QDate> CreateFullRange(const QAbstractItemModel& model, const int column)
{
	const auto allValues        = model.data({}, Role::AuthorsAll + column).value<QVariantList>();
	const auto [minVar, maxVar] = std::ranges::minmax(allValues, {}, [](const auto& item) {
		return item.toDate();
	});

	return std::make_pair(minVar.toDate(), maxVar.toDate());
}

}

class DateIntervalFilterWidget::Impl final : public QObject
{
public:
	Impl(QWidget* self, const QAbstractItemModel& model, const int column, Callback callback, const IParentWidgetProvider& parentWidgetProvider, std::shared_ptr<ISettings> settings)
		: m_column { column }
		, m_callback { std::move(callback) }
		, m_settings { std::move(settings) }
		, m_fullRange { CreateFullRange(model, column) }
	{
		m_ui.setupUi(self);
		self->setFont(parentWidgetProvider.GetWidget()->font());

		m_ui.from->setDateRange(m_fullRange.first, m_fullRange.second);
		m_ui.to->setDateRange(m_fullRange.first, m_fullRange.second);

		connect(m_ui.from, &QCalendarWidget::clicked, m_ui.to, &QCalendarWidget::setMinimumDate);
		connect(m_ui.to, &QCalendarWidget::clicked, m_ui.from, &QCalendarWidget::setMaximumDate);
		connect(m_ui.buttonBox, &QDialogButtonBox::clicked, this, &Impl::OnDialogButtonClicked);
		connect(m_ui.btnReset, &QAbstractButton::clicked, this, &Impl::OnResetClicked);
		connect(m_ui.btnLoad, &QAbstractButton::clicked, this, &Impl::OnLoadClicked);
		connect(m_ui.btnSave, &QAbstractButton::clicked, this, &Impl::OnSaveClicked);

		if (const auto currentFilter = model.data({}, Role::AuthorFilter + m_column).value<const std::unordered_set<QVariant, Util::VariantHash>*>(); !currentFilter->empty())
		{
			const auto [min, max] = currentFilter->begin()->value<std::pair<QDate, QDate>>();
			SetInterval(min, max);
		}
		else
		{
			OnResetClicked();
		}

		const auto btnSize      = m_ui.buttonBox->button(QDialogButtonBox::Cancel)->size();
		const auto calendarSize = m_ui.from->sizeHint();
		const auto margins      = self->layout()->contentsMargins();
		self->setFixedSize(calendarSize.width() * 2 + margins.left() + margins.right() + 6, calendarSize.height() + btnSize.height() + margins.top() + margins.bottom() + self->layout()->spacing() + 6);

		m_ui.btnLoad->setEnabled(Deserialize(m_settings->Get(QString(Constant::Settings::FAST_FILTER_KEY_TEMPLATE).arg(m_column)).toByteArray(), true));
	}

private:
	void OnResetClicked() const
	{
		m_ui.from->setSelectedDate(m_fullRange.first);
		m_ui.to->setSelectedDate(m_fullRange.second);
	}

	void OnDialogButtonClicked(QAbstractButton* button) const
	{
		QVariantList result;
		auto         range = std::make_pair(m_ui.from->selectedDate(), m_ui.to->selectedDate());
		if (m_fullRange != range)
		{
			range.second = range.second.addDays(1);
			result.push_back(QVariant::fromValue(range));
		}

		const auto role = m_ui.buttonBox->buttonRole(button);
		return m_callback(role != QDialogButtonBox::RejectRole, result);
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
		return QJsonDocument(
				   QJsonObject {
					   { MIN_KEY, m_ui.from->selectedDate().toString(DATE_FORMAT) },
					   { MAX_KEY,   m_ui.to->selectedDate().toString(DATE_FORMAT) },
        }
		)
		    .toJson(QJsonDocument::Compact);
	}

	bool Deserialize(const QByteArray& bytes, const bool checkOnly = false) const
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

		if (!doc.isObject())
		{
			PLOGW << "value must be an object";
			return false;
		}

		const auto obj = doc.object();
		const auto min = QDate::fromString(obj.value(MIN_KEY).toString(), DATE_FORMAT);
		if (!min.isValid())
		{
			PLOGW << "min value must be integer";
			return false;
		}
		const auto max = QDate::fromString(obj.value(MAX_KEY).toString(), DATE_FORMAT);
		if (!max.isValid())
		{
			PLOGW << "max value must be integer";
			return false;
		}

		if (checkOnly)
			return true;

		SetInterval(min, max);

		return true;
	}

	void SetInterval(const QDate& min, const QDate& max) const
	{
		m_ui.from->setMaximumDate(max);
		m_ui.from->setSelectedDate(min);

		m_ui.to->setMinimumDate(min);
		m_ui.to->setSelectedDate(max);
	}

private:
	const int                                     m_column;
	const Callback                                m_callback;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	std::pair<QDate, QDate>                       m_fullRange;

	Ui::DateIntervalFilterWidget m_ui {};
};

DateIntervalFilterWidget::DateIntervalFilterWidget(
	const QAbstractItemModel&    model,
	int                          column,
	Callback                     callback,
	const IParentWidgetProvider& parentWidgetProvider,
	std::shared_ptr<ISettings>   settings,
	QWidget*                     parent
)
	: QWidget(parent)
	, m_impl { this, model, column, std::move(callback), parentWidgetProvider, std::move(settings) }
{
}

DateIntervalFilterWidget::~DateIntervalFilterWidget() = default;
