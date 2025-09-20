#pragma once

#include <memory>

#include "fnd/NonCopyMovable.h"

#include "interface/logic/IFilterProvider.h"

#include "util/ISettings.h"

namespace HomeCompa::Flibrary
{

class FilterController final : public IFilterController
{
	NON_COPY_MOVABLE(FilterController)

public:
	explicit FilterController(std::shared_ptr<ISettings> settings);
	~FilterController() override;

private: // IFilterProvider
	[[nodiscard]] bool IsFilterEnabled() const noexcept override;

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private: // IFilterController
	void SetFilterEnabled(bool enabled) override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
