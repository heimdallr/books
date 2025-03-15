#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IModel.h"

namespace HomeCompa::Flibrary
{

class GenreModel final : public IGenreModel
{
	NON_COPY_MOVABLE(GenreModel)

public:
	explicit GenreModel(std::shared_ptr<const IDatabaseUser> databaseUser);
	~GenreModel() override;

protected: // ILanguageMode
	QAbstractItemModel* GetModel() noexcept override;

protected:
	class Model;
	PropagateConstPtr<Model> m_model;
};

}
