#pragma once

#include <functional>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "interface/logic/IDataItem.h"

class QModelIndex;

namespace HomeCompa::Flibrary {

class BooksContextMenuProvider final
{
	NON_COPY_MOVABLE(BooksContextMenuProvider)

public:
	using Callback = std::function<void(const IDataItem::Ptr &)>;

public:
	explicit BooksContextMenuProvider(std::shared_ptr<class DatabaseUser> databaseUser);
	~BooksContextMenuProvider();

public:
	void Request(const QModelIndex & index, Callback callback);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
