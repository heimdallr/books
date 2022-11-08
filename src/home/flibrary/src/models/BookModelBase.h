#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "Book.h"
#include "BookRole.h"
#include "ModelObserver.h"
#include "ProxyModelBaseT.h"

namespace HomeCompa::Flibrary {

class BookModelBase
	: public ProxyModelBaseT<Book, BookRole, ModelObserver>
{
	NON_COPY_MOVABLE(BookModelBase)

protected:
	BookModelBase(QSortFilterProxyModel & proxyModel, Items & items);
	~BookModelBase() override;

protected: // ProxyModelBaseT
	bool FilterAcceptsRow(const int row, const QModelIndex & parent = {}) const override;
	QVariant GetDataGlobal(int role) const override;
	bool SetDataGlobal(const QVariant & value, int role) override;
	void Reset() override;

protected:
	bool m_showDeleted { false };
	QString m_languageFilter;
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
