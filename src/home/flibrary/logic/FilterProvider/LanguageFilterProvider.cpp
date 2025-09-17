#include "LanguageFilterProvider.h"

#include <QString>

#include "fnd/observable.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto FILTERED_LANGUAGES_KEY = "ui/Books/LanguageFilter/languages";
constexpr auto LANGUAGES_FILTER_ENABLED_KEY = "ui/Books/LanguageFilter/enabled";
}

struct LanguageFilterProvider::Impl final : Observable<IObserver>
{
	std::shared_ptr<ISettings> settings;

	std::unordered_map<QString, QString> nameToCode;
	std::unordered_map<QString, QString> codeToName;

	explicit Impl(std::shared_ptr<ISettings> settings)
		: settings { std::move(settings) }
	{
	}
};

LanguageFilterProvider::LanguageFilterProvider(std::shared_ptr<ISettings> settings)
	: m_impl { std::make_unique<Impl>(std::move(settings)) }
{
}

LanguageFilterProvider::~LanguageFilterProvider() = default;

bool LanguageFilterProvider::IsFilterEnabled() const
{
	return m_impl->settings->Get(LANGUAGES_FILTER_ENABLED_KEY, false);
}

std::unordered_set<QString> LanguageFilterProvider::GetFilteredCodes() const
{
	auto list = m_impl->settings->Get(FILTERED_LANGUAGES_KEY, QString {}).split(';');
	std::unordered_set<QString> filtered { std::make_move_iterator(list.begin()), std::make_move_iterator(list.end()) };
	return filtered;
}

void LanguageFilterProvider::SetEnabled(const bool enabled)
{
	m_impl->settings->Set(LANGUAGES_FILTER_ENABLED_KEY, enabled);
	m_impl->Perform(&IObserver::OnFilterChanged);
}

void LanguageFilterProvider::SetFilteredCodes(const bool enabled, const std::unordered_set<QString>& codes)
{
	m_impl->settings->Set(LANGUAGES_FILTER_ENABLED_KEY, enabled);
	m_impl->settings->Set(FILTERED_LANGUAGES_KEY, QStringList { codes.cbegin(), codes.cend() }.join(';'));
	m_impl->Perform(&IObserver::OnFilterChanged);
}

IFilterProvider& LanguageFilterProvider::ToProvider() noexcept
{
	return *this;
}

void LanguageFilterProvider::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void LanguageFilterProvider::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
