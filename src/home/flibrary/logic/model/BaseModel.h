#pragma once

#include <QAbstractItemModel>

#include "fnd/NonCopyMovable.h"

#include "interface/logic/IDataItem.h"

namespace HomeCompa::Flibrary
{
class ILibRateProvider;

class BaseModel : public QAbstractItemModel
{
	NON_COPY_MOVABLE(BaseModel)

protected:
	explicit BaseModel(const std::shared_ptr<class IModelProvider>& modelProvider, QObject* parent);
	~BaseModel() override;

protected: // QAbstractItemModel
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	bool setData(const QModelIndex& index, const QVariant& value, int role) override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;

protected:
	virtual IDataItem* GetInternalPointer(const QModelIndex& index) const;

protected:
	static QVariant GetValue(const IDataItem& item, const int column);

protected:
	IDataItem::Ptr m_data;
	std::shared_ptr<const ILibRateProvider> m_libRateProvider;
	bool m_checkable { false };
};

} // namespace HomeCompa::Flibrary
