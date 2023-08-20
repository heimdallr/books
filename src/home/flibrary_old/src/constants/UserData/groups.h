#pragma once

namespace HomeCompa::Flibrary::Constant::UserData::Groups {

constexpr auto RootNode  = "Groups";
constexpr auto GroupNode = "Group";

constexpr auto CreateNewGroupCommandText = "insert into Groups_User(Title) values(?)";
constexpr auto AddBookToGroupCommandText = "insert into Groups_List_User(BookID, GroupID) values(?, ?)";

}
