#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IOpdsController.h"

namespace HomeCompa::Flibrary
{

class OpdsController final : public IOpdsController
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

	bool InStartup() const override;
	void AddToStartup() const override;
	void RemoveFromStartup() const override;

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
