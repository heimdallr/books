#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/IOpdsController.h"

namespace HomeCompa::Flibrary {

class OpdsController final
	: public IOpdsController
{
	NON_COPY_MOVABLE(OpdsController)

public:
	OpdsController();
	~OpdsController() override;

private: // IOpdsController
	bool IsRunning() const override;
	void Start() override;
	void Stop() override;
	void Restart() override;

	void RegisterObserver(IObserver * observer) override;
	void UnregisterObserver(IObserver * observer) override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
