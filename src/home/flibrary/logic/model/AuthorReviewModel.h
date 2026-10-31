#pragma once

#include <QSortFilterProxyModel>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseProvider.h"

#include "settings/ISettings.h"

namespace HomeCompa::Flibrary {

class AuthorReviewModel final : public QSortFilterProxyModel
{
	NON_COPY_MOVABLE(AuthorReviewModel)

public:
	AuthorReviewModel(
		const std::shared_ptr<const ISettings>&           settings,
		const std::shared_ptr<const ICollectionProvider>& collectionProvider,
		std::shared_ptr<const IDatabaseProvider>          databaseProvider,
		QObject*                                          parent = nullptr
	);
	~AuthorReviewModel() override;

private:
	PropagateConstPtr<QAbstractItemModel> m_source;
};

} // namespace HomeCompa::Flibrary
