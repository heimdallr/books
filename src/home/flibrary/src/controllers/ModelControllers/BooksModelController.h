#pragma once

#include "fnd/memory.h"

#include "ModelController.h"

#include "models/Book.h"

namespace HomeCompa::Util {
class Executor;
}

namespace HomeCompa::DB {
class Database;
}

namespace HomeCompa::Flibrary {

class BooksModelController
	: public ModelController
{
	NON_COPY_MOVABLE(BooksModelController)
public:
	BooksModelController(Util::Executor & executor, DB::Database & db);
	~BooksModelController() override;

	void SetNavigationId(const QString & navigationType, int navigationId);

private: // ModelController
	Type GetType() const noexcept override;
	QAbstractItemModel * GetModelImpl(const QString & modelType) override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
