#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IAuthorAnnotationController.h"
#include "interface/logic/IModelProvider.h"

#include "ListModel.h"

namespace HomeCompa::Flibrary
{

class AuthorsModel final : public ListModel
{
	NON_COPY_MOVABLE(AuthorsModel)

public:
	AuthorsModel(const std::shared_ptr<IModelProvider>& modelProvider, std::shared_ptr<IAuthorAnnotationController> authorAnnotationController, QObject* parent = nullptr);
	~AuthorsModel() override;

private: // QAbstractItemModel
	int      columnCount(const QModelIndex& parent) const override;
	QVariant data(const QModelIndex& index, int role) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
