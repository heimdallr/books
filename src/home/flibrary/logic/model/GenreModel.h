#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IDatabaseProvider.h"
#include "interface/logic/IModel.h"

namespace HomeCompa::Flibrary {

class GenreModel final : public IGenreModel
{
	NON_COPY_MOVABLE(GenreModel)

public:
	explicit GenreModel(std::shared_ptr<const IDatabaseProvider> databaseProvider);
	~GenreModel() override;

protected: // ILanguageMode
	QAbstractItemModel* GetModel() noexcept override;

protected:
	PropagateConstPtr<QAbstractItemModel> m_model;
};

} // namespace HomeCompa::Flibrary
