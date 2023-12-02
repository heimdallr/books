#pragma once

#include "fnd/observer.h"

namespace HomeCompa::Flibrary {

class IProgressController  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IObserver : public Observer
	{
	public:
		virtual void OnStartedChanged() = 0;
		virtual void OnValueChanged() = 0;
		virtual void OnStop() = 0;
	};

	class IProgressItem  // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~IProgressItem() = default;
		virtual void Increment(int64_t value) = 0;
		virtual bool IsStopped() const noexcept = 0;
	};

public:
	virtual ~IProgressController() = default;

	virtual bool IsStarted() const noexcept = 0;
	virtual std::unique_ptr<IProgressItem> Add(int64_t value) = 0;
	virtual double GetValue() const noexcept = 0;
	virtual void Stop() = 0;

	virtual void RegisterObserver(IObserver * observer) = 0;
	virtual void UnregisterObserver(IObserver * observer) = 0;
};

class IBooksExtractorProgressController : virtual public IProgressController
{
};

}
