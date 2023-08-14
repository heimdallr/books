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
	ProgressItem(IObserver & observer, const int64_t maximum)
		: m_observer(observer)
		, m_maximum(maximum)
	{
	}

	~ProgressItem() override
	{
		m_observer.OnIncremented(m_maximum - m_value);
		m_observer.OnDestroyed();
	}

private: // IProgressController::IProgressItem
	void Increment(const int64_t value) override
	{
		m_value += value;
		m_observer.OnIncremented(value);
	}

private:
	IObserver & m_observer;
	const int64_t m_maximum;
	int64_t m_value { 0 };
};

}

struct ProgressController::Impl final
	: Observable<IObserver>
	, ProgressItem::IObserver
{
	int64_t globalMaximum { 0 };
	std::atomic_int64_t count { 0 };
	std::atomic_int64_t globalValue { 0 };
	Util::FunctorExecutionForwarder forwarder;

	void Process()
	{
		Perform(&IProgressController::IObserver::OnValueChanged);
	}

	void OnIncremented(const int64_t value) override
	{
		globalValue += value;
		forwarder.Forward([&] { m_timer->start(); });
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

	PropagateConstPtr<QTimer> m_timer { Util::CreateUiTimer([&]{Process(); })};
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
	++m_impl->count;
	m_impl->globalMaximum += value;
	m_impl->Perform(&IObserver::OnStartedChanged);
	return std::make_unique<ProgressItem>(*m_impl, value);
}

double ProgressController::GetValue() const noexcept
{
	return static_cast<double>(m_impl->globalValue) / static_cast<double>(m_impl->globalMaximum);
}
