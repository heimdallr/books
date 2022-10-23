#pragma once

#include "fnd/memory.h"

#include "ModelController.h"

namespace HomeCompa::Util {
class Executor;
}

namespace HomeCompa::DB {
class Database;
}

namespace HomeCompa::Flibrary {

enum class BooksViewType;
enum class NavigationSource;

class BooksModelController
	: public ModelController
{
	NON_COPY_MOVABLE(BooksModelController)
public:
	BooksModelController(Util::Executor & executor, DB::Database & db, BooksViewType booksViewType);
	~BooksModelController() override;

	void SetNavigationState(NavigationSource navigationSource, const QString & navigationId);

private: // ModelController
	Type GetType() const noexcept override;
	QAbstractItemModel * CreateModel() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
