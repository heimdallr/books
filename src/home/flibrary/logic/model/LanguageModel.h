#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IModel.h"

namespace HomeCompa::Flibrary
{

class LanguageModel final : public ILanguageModel
{
	NON_COPY_MOVABLE(LanguageModel)

public:
	explicit LanguageModel(std::shared_ptr<const IDatabaseUser> databaseUser);
	~LanguageModel() override;

protected: // ILanguageMode
	QAbstractItemModel* GetModel() noexcept override;

protected:
	class Model;
	PropagateConstPtr<Model> m_model;
};

}
