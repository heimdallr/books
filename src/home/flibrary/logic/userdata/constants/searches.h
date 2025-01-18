#pragma once

namespace HomeCompa::Flibrary::Constant::UserData::Searches {

constexpr auto RootNode = "Searches";
constexpr auto Mode = "Mode";

constexpr auto CreateNewSearchCommandText = "insert into Searches_User(Title, Mode, SearchTitle, CreatedAt) values(?, ?, ?, ?)";

}
