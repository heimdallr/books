#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/IModel.h"

namespace HomeCompa::Flibrary {

class LanguageModel final : public ILanguageModel
{
	NON_COPY_MOVABLE(LanguageModel)

public:
	explicit LanguageModel(std::shared_ptr<const class IDatabaseUser> databaseUser);
	~LanguageModel() override;

protected: // ILanguageMode
	QAbstractItemModel* GetModel() noexcept override;

protected:
	class Model;
	PropagateConstPtr<Model> m_model;
};

}
