#pragma once

#include "fnd/NonCopyMovable.h"

#include "interface/logic/IModelProvider.h"

#include "ListModel.h"

namespace HomeCompa::Flibrary
{

class AuthorsModel final : public ListModel
{
	NON_COPY_MOVABLE(AuthorsModel)

public:
	explicit AuthorsModel(const std::shared_ptr<IModelProvider>& modelProvider, QObject* parent = nullptr);
	~AuthorsModel() override;

private: // QAbstractItemModel
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
};

} // namespace HomeCompa::Flibrary
