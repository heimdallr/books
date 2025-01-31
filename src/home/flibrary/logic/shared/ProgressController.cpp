#include "ProgressController.h"

#include <atomic>

#include <QTimer>

#include "fnd/observable.h"
#include "util/UiTimer.h"
#include "util/FunctorExecutionForwarder.h"

#include <plog/Log.h>

#include "interface/logic/IProgressController.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

class ProgressItem final : public IProgressController::IProgressItem
{
	NON_COPY_MOVABLE(ProgressItem)
public:
	class IObserver  // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~IObserver() = default;
		virtual void OnIncremented(int64_t value) = 0;
		virtual void OnDestroyed() = 0;
	};

public:
	ProgressItem(std::atomic_bool & stopped, IObserver & observer, const int64_t maximum)
		: m_stopped(stopped)
		, m_observer(observer)
		, m_maximum(maximum)
	{
	}

	~ProgressItem() override
	{
		m_observer.OnIncremented(m_maximum - m_value);
		m_observer.OnDestroyed();
	}

private: // IProgressController::IProgressItem
	void Increment(int64_t value) override
	{
		value = std::min(value, m_maximum - m_value);
		m_value += value;
		if (value)
			m_observer.OnIncremented(value);
	}

	bool IsStopped() const noexcept override
	{
		return m_stopped;
	}

private:
	std::atomic_bool & m_stopped;
	IObserver & m_observer;
	const int64_t m_maximum;
	int64_t m_value { 0 };
};

}

struct ProgressController::Impl final
	: Observable<IObserver>
	, ProgressItem::IObserver
{
	std::atomic_bool stopped { false };
	int64_t globalMaximum { 0 };
	std::atomic_int64_t count { 0 };
	std::atomic_int64_t globalValue { 0 };
	Util::FunctorExecutionForwarder forwarder;

	void OnIncremented(const int64_t value) override
	{
		if (stopped)
			return;

		globalValue += value;
		forwarder.Forward([&]
		{
			Perform(&IProgressController::IObserver::OnValueChanged);
		});
	}

	void OnDestroyed() override
	{
		if (--count != 0)
			return;

		forwarder.Forward([&, maximum = globalMaximum, value = globalValue.load()]
		{
			globalMaximum -= maximum;
			globalValue -= value;
			Perform(&IProgressController::IObserver::OnStartedChanged);
		});
	}
};

ProgressController::ProgressController()
{
	PLOGD << "ProgressController created";
}

ProgressController::~ProgressController()
{
	PLOGD << "ProgressController destroyed";
}

bool ProgressController::IsStarted() const noexcept
{
	return m_impl->count != 0;
}

void ProgressController::RegisterObserver(IObserver * observer)
{
	m_impl->Register(observer);
}

void ProgressController::UnregisterObserver(IObserver * observer)
{
	m_impl->Unregister(observer);
}

std::unique_ptr<IProgressController::IProgressItem> ProgressController::Add(const int64_t value)
{
	const auto justStarted = m_impl->count == 0;
	++m_impl->count;
	m_impl->globalMaximum += value;
	if (justStarted)
	{
		m_impl->stopped = false;
		m_impl->forwarder.Forward([&]
		{
			m_impl->Perform(&IObserver::OnStartedChanged);
		});
	}
	return std::make_unique<ProgressItem>(m_impl->stopped, *m_impl, value);
}

double ProgressController::GetValue() const noexcept
{
	return static_cast<double>(m_impl->globalValue) / static_cast<double>(m_impl->globalMaximum);
}

void ProgressController::Stop()
{
	m_impl->stopped = true;
	m_impl->Perform(&IObserver::OnStop);
}
