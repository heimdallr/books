#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/ISingleInstanceController.h"

#include "util/ISettings.h"

namespace HomeCompa::Flibrary
{

class SingleInstanceController final : virtual public ISingleInstanceController
{
	NON_COPY_MOVABLE(SingleInstanceController)

public:
	explicit SingleInstanceController(std::shared_ptr<const ISettings> settings);
	~SingleInstanceController() override;

private: // ISingleInstanceController
	bool IsFirstSingleInstanceApp() const override;

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
