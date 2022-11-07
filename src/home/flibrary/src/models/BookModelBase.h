#pragma once

#include "Book.h"
#include "BookRole.h"
#include "ModelObserver.h"
#include "ProxyModelBaseT.h"

namespace HomeCompa::Flibrary {

class BookModelBase
	: public ProxyModelBaseT<Book, BookRole, ModelObserver>
{
protected:
	BookModelBase(QSortFilterProxyModel & proxyModel, Items & items);

protected: // ProxyModelBaseT
	bool FilterAcceptsRow(const int row, const QModelIndex & parent = {}) const override;
	QVariant GetDataGlobal(int role) const override;
	bool SetDataGlobal(const QVariant & value, int role) override;
	void Reset() override;

protected:
	bool m_showDeleted { false };
	QString m_languageFilter;
};

}
