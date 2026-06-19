#include "ui_RangeFilterWidget.h"

#include "RangeFilterWidget.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>

#include "fnd/algorithm.h"

#include "interface/constants/ModelRole.h"
#include "interface/constants/SettingsConstant.h"

#include "gutil/interface/IParentWidgetProvider.h"
#include "settings/ISettings.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

namespace
{

constexpr auto MIN_KEY = "min";
constexpr auto MAX_KEY = "max";

std::pair<int, int> CreateFullRange(const QAbstractItemModel& model, const int column)
{
	const auto allValues        = model.data({}, Role::AuthorsAll + column).value<QVariantList>();
	const auto [minVar, maxVar] = std::ranges::minmax(allValues, {}, [](const auto& item) {
		return item.toULongLong();
	});

	return std::make_pair(minVar.toInt() / 1024, (maxVar.toInt() + 1023) / 1024);
}

}

class RangeFilterWidget::Impl final : public QObject
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

		const auto [min, max] = m_fullRange;

		m_ui.labelMin->setText(tr("%1 kB").arg(min));
		m_ui.labelMax->setText(tr("%1 kB").arg(max));

		m_ui.slider->setMinimum(min);
		m_ui.slider->setMaximum(max);

		m_ui.slider->setLow(min);
		m_ui.slider->setHigh(max);

		m_ui.spinBoxMin->setMinimum(min);
		m_ui.spinBoxMax->setMinimum(min);
		m_ui.spinBoxMin->setMaximum(max);
		m_ui.spinBoxMax->setMaximum(max);

		connect(m_ui.slider, &RangeSlider::sliderMoved, this, &Impl::OnSliderMoved);
		connect(m_ui.spinBoxMin, qOverload<int>(&QSpinBox::valueChanged), m_ui.spinBoxMax, &QSpinBox::setMinimum);
		connect(m_ui.spinBoxMax, qOverload<int>(&QSpinBox::valueChanged), m_ui.spinBoxMin, &QSpinBox::setMaximum);
		connect(m_ui.spinBoxMin, qOverload<int>(&QSpinBox::valueChanged), m_ui.slider, &RangeSlider::setLow);
		connect(m_ui.spinBoxMax, qOverload<int>(&QSpinBox::valueChanged), m_ui.slider, &RangeSlider::setHigh);
		connect(m_ui.buttonBox, &QDialogButtonBox::clicked, this, &Impl::OnDialogButtonClicked);
		connect(m_ui.btnReset, &QAbstractButton::clicked, this, &Impl::OnResetClicked);
		connect(m_ui.btnLoad, &QAbstractButton::clicked, this, &Impl::OnLoadClicked);
		connect(m_ui.btnSave, &QAbstractButton::clicked, this, &Impl::OnSaveClicked);

		if (const auto currentFilter = model.data({}, Role::AuthorFilter + m_column).value<const std::unordered_set<QVariant, Util::VariantHash>*>(); !currentFilter->empty())
		{
			const auto [currentMin, currentMax] = currentFilter->begin()->value<std::pair<int, int>>();
			OnSliderMoved(currentMin, currentMax);
		}
		else
		{
			OnResetClicked();
		}

		const auto btnSize = m_ui.buttonBox->button(QDialogButtonBox::Cancel)->size();
		self->setFixedSize(7 * btnSize.width() / 2, 4 * (btnSize.height() + 6));

		m_ui.btnLoad->setEnabled(Deserialize(m_settings->Get(QString(Constant::Settings::FAST_FILTER_KEY_TEMPLATE).arg(m_column)).toByteArray(), true));
	}

private:
	void OnResetClicked() const
	{
		m_ui.spinBoxMin->setValue(m_fullRange.first);
		m_ui.spinBoxMax->setValue(m_fullRange.second);
	}

	void OnSliderMoved(const int min, const int max) const
	{
		m_ui.spinBoxMin->setValue(min);
		m_ui.spinBoxMax->setValue(max);
	}

	void OnDialogButtonClicked(QAbstractButton* button) const
	{
		QVariantList result;
		const auto   range = std::make_pair(m_ui.slider->low(), m_ui.slider->high());
		if (m_fullRange != range)
			result.push_back(QVariant::fromValue(range));

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
					   { MIN_KEY,  m_ui.slider->low() },
					   { MAX_KEY, m_ui.slider->high() },
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
		const auto min = obj.value(MIN_KEY).toInt(-1);
		if (min < 0)
		{
			PLOGW << "min value must be integer";
			return false;
		}
		const auto max = obj.value(MAX_KEY).toInt(-1);
		if (max < 0)
		{
			PLOGW << "max value must be integer";
			return false;
		}

		if (checkOnly)
			return true;

		OnSliderMoved(min, max);

		return true;
	}

private:
	const int                                     m_column;
	const Callback                                m_callback;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	std::pair<int, int>                           m_fullRange;

	Ui::RangeFilterWidget m_ui {};
};

RangeFilterWidget::RangeFilterWidget(
	const QAbstractItemModel&    model,
	const int                    column,
	Callback                     callback,
	const IParentWidgetProvider& parentWidgetProvider,
	std::shared_ptr<ISettings>   settings,
	QWidget*                     parent
)
	: QWidget(parent)
	, m_impl(this, model, column, std::move(callback), parentWidgetProvider, std::move(settings))
{
}

RangeFilterWidget::~RangeFilterWidget() = default;
