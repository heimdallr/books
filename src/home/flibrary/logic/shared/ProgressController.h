#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IProgressController.h"

namespace HomeCompa::Flibrary
{

class ProgressController final
	: public IAnnotationProgressController
	, public IBooksExtractorProgressController
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

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
