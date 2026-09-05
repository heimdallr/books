#pragma once

namespace HomeCompa::Flibrary::Constant::UserData::Filter
{

constexpr auto RootNode = "Filter";
constexpr auto Title    = L"Title";
constexpr auto Flag     = L"Flag";

static constexpr std::string_view FIELD_NAMES[] { "LastName||','||FirstName||','||MiddleName", "SeriesTitle", "FB2Code", "", "KeywordTitle", "", "", "LanguageCode", "Title", "", "", "", "", "" };
static_assert(static_cast<size_t>(NavigationMode::Last) == std::size(FIELD_NAMES));

}
