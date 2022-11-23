#include <QAbstractItemModel>
#include <QQmlEngine>

#include "fnd/observable.h"

#include "ComboBoxController.h"

namespace HomeCompa::Flibrary {

struct ComboBoxController::Impl final
	: Observable<ComboBoxObserver>
{
	ComboBoxDataProvider & dataProvider;
	SimpleModelItems simpleModelItems;
	QAbstractItemModel * const model;

	explicit Impl(ComboBoxDataProvider & dataProvider_, QObject * parent)
		: dataProvider(dataProvider_)
		, model(CreateSimpleModel(simpleModelItems, parent))
	{
		QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
	}
};

ComboBoxController::ComboBoxController(ComboBoxDataProvider & dataProvider, QObject * parent)
	: QObject(parent)
	, m_impl(dataProvider, this)
{
}

ComboBoxController::~ComboBoxController() = default;

QAbstractItemModel * ComboBoxController::GetModel() const
{
	return m_impl->model;
}

void ComboBoxController::SetData(SimpleModelItems simpleModelItems)
{
	m_impl->simpleModelItems = std::move(simpleModelItems);
	m_impl->model->setData({}, QVariant::fromValue(&m_impl->simpleModelItems), SimpleModelRole::SetItems);
	OnValueChanged();
}

void ComboBoxController::OnValueChanged() const
{
	emit ValueChanged();
	emit TitleChanged();
}

void ComboBoxController::RegisterObserver(ComboBoxObserver * observer)
{
	m_impl->Register(observer);
}

void ComboBoxController::UnregisterObserver(ComboBoxObserver * observer)
{
	m_impl->Unregister(observer);
}

const QString & ComboBoxController::GetTitle() const
{
	const auto it = std::ranges::find_if(std::as_const(m_impl->simpleModelItems), [value = GetValue()](const SimpleModelItem & item)
	{
		return item.Value == value;
	});
	assert(it != std::cend(m_impl->simpleModelItems));
	return it->Title;
}

QString ComboBoxController::GetValue() const
{
	return m_impl->dataProvider.GetValue();
}

void ComboBoxController::SetValue(const QString & value)
{
	m_impl->Perform(&ComboBoxObserver::SetValue, std::cref(value));
}

}
