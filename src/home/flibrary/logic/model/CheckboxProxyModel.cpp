#include "CheckboxProxyModel.h"

#include "interface/constants/ModelRole.h"

#include "data/AbstractModelProvider.h"

using namespace HomeCompa::Flibrary;

AbstractCheckboxProxyModel::AbstractCheckboxProxyModel(QObject * parent)
	: QIdentityProxyModel(parent)
{
}

CheckboxProxyModel::CheckboxProxyModel(const std::shared_ptr<class AbstractModelProvider> & modelProvider, QObject * parent)
	: AbstractCheckboxProxyModel(parent)
	, m_sourceModel(modelProvider->GetSourceModel())
{
	QIdentityProxyModel::setSourceModel(m_sourceModel.get());
}

CheckboxProxyModel::~CheckboxProxyModel() = default;

bool CheckboxProxyModel::setData(const QModelIndex & index, const QVariant & value, const int role)
{
	if (index.isValid())
	{
		switch (role)
		{
			case Qt::CheckStateRole:
				if (const auto checkState = index.data(Qt::CheckStateRole); checkState.isValid())
				{
					Check(index, checkState.value<Qt::CheckState>() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
					auto parent = index;
					while (parent.isValid())
					{
						emit dataChanged(parent, parent, { Qt::CheckStateRole });
						parent = parent.parent();
					}
					return true;
				}

				return false;

			default:
				break;
		}
	}

	return m_sourceModel->setData(mapToSource(index), value, role);
}


void CheckboxProxyModel::Check(const QModelIndex & parent, const Qt::CheckState state)
{
	const auto count = rowCount(parent);
	for (auto i = 0; i < count; ++i)
		Check(index(i, 0, parent), state);

	emit dataChanged(index(0, 0, parent), index(count - 1, 0), { Qt::CheckStateRole });

	setData(parent, state, Role::CheckState);
}
