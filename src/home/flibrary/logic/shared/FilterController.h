#pragma once

#include <memory>

#include "fnd/NonCopyMovable.h"

#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IFilterController.h"

#include "util/ISettings.h"

namespace HomeCompa::Flibrary
{

class FilterController final : public IFilterController
{
	NON_COPY_MOVABLE(FilterController)

public:
	FilterController(std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<ISettings> settings);
	~FilterController() override;

private: // IFilterProvider
	[[nodiscard]] bool                          IsFilterEnabled() const noexcept override;
	[[nodiscard]] std::vector<IDataItem::Flags> GetFlags(const NavigationMode navigationMode, const std::vector<QString>& ids) const override;

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private: // IFilterController
	void SetFilterEnabled(bool enabled) override;
	void Apply() override;
	void SetFlags(NavigationMode navigationMode, QString id, IDataItem::Flags flags) override;
	void SetNavigationItemFlags(NavigationMode navigationMode, QStringList navigationIds, IDataItem::Flags flags, Callback callback) override;
	void ClearNavigationItemFlags(NavigationMode navigationMode, QStringList navigationIds, IDataItem::Flags flags, Callback callback) override;
	void HideFiltered(NavigationMode navigationMode, QPointer<QAbstractItemModel> model, std::weak_ptr<ICallback> callback) override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
