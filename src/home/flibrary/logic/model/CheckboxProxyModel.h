#pragma once

#include <QIdentityProxyModel>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "data/DataItem.h"

namespace HomeCompa::Flibrary {

class AbstractCheckboxProxyModel : public QIdentityProxyModel
{
protected:
	explicit AbstractCheckboxProxyModel(QObject * parent = nullptr);
};

class CheckboxProxyModel final : public AbstractCheckboxProxyModel
{
	NON_COPY_MOVABLE(CheckboxProxyModel)

public:
	explicit CheckboxProxyModel(const std::shared_ptr<class AbstractModelProvider> & modelProvider, QObject * parent = nullptr);
	~CheckboxProxyModel() override;

private: // QAbstractItemModel
	bool setData(const QModelIndex& index, const QVariant& value, int role) override;

private:
	void Check(const QModelIndex & parent, Qt::CheckState state);

private:
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_sourceModel;
};

}
