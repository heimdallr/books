#pragma once

namespace HomeCompa::Flibrary::Constant::UserData::Searches {

constexpr auto RootNode = "Searches";
constexpr auto Origin    = u"Origin";

constexpr auto CreateNewSearchCommandText = "insert into Searches_User(Origin, Title, CreatedAt) values(?, ?, ?)";

} // namespace HomeCompa::Flibrary::Constant::UserData::Searches
