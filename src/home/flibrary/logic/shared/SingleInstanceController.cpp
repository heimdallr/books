#include "SingleInstanceController.h"

#include "fnd/observable.h"

using namespace HomeCompa::Flibrary;

struct SingleInstanceController::Impl final : Observable<ISingleInstanceController::IObserver>
{
	std::shared_ptr<const ISettings> settings;

	explicit Impl(std::shared_ptr<const ISettings> settings)
		: settings { std::move(settings) }
	{
	}
};

SingleInstanceController::SingleInstanceController(std::shared_ptr<const ISettings> settings)
	: m_impl { std::move(settings) }
{
}

SingleInstanceController::~SingleInstanceController() = default;

bool SingleInstanceController::IsFirstSingleInstanceApp() const
{
	return false;
}

void SingleInstanceController::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void SingleInstanceController::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
