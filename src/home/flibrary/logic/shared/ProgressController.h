#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/IProgressController.h"

namespace HomeCompa::Flibrary {

class ProgressController : virtual public IProgressController
{
	NON_COPY_MOVABLE(ProgressController)

public:
	ProgressController();
	~ProgressController() override;

private: // IProgressController
	bool IsStarted() const noexcept override;
	std::unique_ptr<IProgressItem> Add(int64_t value) override;
	double GetValue() const noexcept override;
	void Stop() override;

	void RegisterObserver(IObserver * observer) override;
	void UnregisterObserver(IObserver * observer) override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
