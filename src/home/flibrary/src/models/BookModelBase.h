#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "Book.h"
#include "BookRole.h"
#include "BookModelObserver.h"
#include "ProxyModelBaseT.h"

namespace HomeCompa::Flibrary {

class BookModelBase
	: public ProxyModelBaseT<Book, BookRole, BookModelObserver>
{
	NON_COPY_MOVABLE(BookModelBase)

protected:
	BookModelBase(QSortFilterProxyModel & proxyModel, Items & items);
	~BookModelBase() override;

protected:
	bool ChangeCheckedBook(int index, const std::function<bool(const Book &)> & f, std::set<int> & changed);

protected: // ProxyModelBaseT
	bool FilterAcceptsRow(const int row, const QModelIndex & parent = {}) const override;
	QVariant GetDataGlobal(int role) const override;
	bool SetDataGlobal(const QVariant & value, int role) override;
	void Reset() override;

private:
	bool RemoveImpl(long long id, bool remove);
	bool ChangeCheckedAll(const std::function<bool(const Book &)> & f);

private:
	virtual bool ChangeCheckedDictionary(int index, const std::function<bool(const Book &)> & f, std::set<int> & changed);

protected:
	bool m_showDeleted { false };
	QString m_languageFilter;
};

}
