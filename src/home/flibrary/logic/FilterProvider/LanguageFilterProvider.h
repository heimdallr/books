#pragma once

#include <memory>

#include "fnd/NonCopyMovable.h"

#include "interface/logic/IBooksFilterProvider.h"

#include "util/ISettings.h"

namespace HomeCompa::Flibrary
{

class LanguageFilterProvider final
	: public ILanguageFilterProvider
	, public ILanguageFilterController
{
	NON_COPY_MOVABLE(LanguageFilterProvider)

public:
	explicit LanguageFilterProvider(std::shared_ptr<ISettings> settings);
	~LanguageFilterProvider() override;

private: // IFilterProvider
	[[nodiscard]] bool IsFilterEnabled() const override;
	[[nodiscard]] std::unordered_set<QString> GetFilteredCodes() const override;
	//	[[nodiscard]] std::unordered_set<QString> GetFilteredNames() const override;
//	[[nodiscard]] const std::unordered_map<QString, QString>& GetNameToCodeMap() const override;
//	[[nodiscard]] const std::unordered_map<QString, QString>& GetCodeToNameMap() const override;

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private: // IFilterController
	void SetFilteredCodes(bool enabled, const std::unordered_set<QString>& codes) override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
