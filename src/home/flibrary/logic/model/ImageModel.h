#pragma once

#include <QIdentityProxyModel>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseUser.h"

namespace HomeCompa::Flibrary
{

class ImageModel final : public QIdentityProxyModel
{
	NON_COPY_MOVABLE(ImageModel)

public:
	ImageModel(std::shared_ptr<const ICollectionProvider> collectionProvider, std::shared_ptr<const IDatabaseUser> databaseUser);
	~ImageModel() override;

protected:
	PropagateConstPtr<QAbstractItemModel> m_model;
};

}
