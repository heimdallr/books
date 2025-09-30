#pragma once

#include <QSortFilterProxyModel>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseUser.h"

#include "util/ISettings.h"

namespace HomeCompa::Flibrary
{

class AuthorReviewModel final : public QSortFilterProxyModel
{
	NON_COPY_MOVABLE(AuthorReviewModel)

public:
	AuthorReviewModel(
		const std::shared_ptr<const ISettings>&           settings,
		const std::shared_ptr<const ICollectionProvider>& collectionProvider,
		std::shared_ptr<const IDatabaseUser>              databaseUser,
		QObject*                                          parent = nullptr
	);
	~AuthorReviewModel() override;

private:
	PropagateConstPtr<QAbstractItemModel> m_source;
};

} // namespace HomeCompa::Flibrary
