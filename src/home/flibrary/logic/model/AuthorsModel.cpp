#include "AuthorsModel.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

AuthorsModel::AuthorsModel(const std::shared_ptr<IModelProvider>& modelProvider, QObject* parent)
	: ListModel(modelProvider, parent)
{
	PLOGV << "AuthorsModel created";
}

AuthorsModel::~AuthorsModel()
{
	PLOGV << "AuthorsModel destroyed";
}

int AuthorsModel::columnCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : 1;
}
