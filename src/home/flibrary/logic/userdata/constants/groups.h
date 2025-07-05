#pragma once

namespace HomeCompa::Flibrary::Constant::UserData::Groups
{

constexpr auto RootNode = "Groups";
constexpr auto GroupNode = "Group";

constexpr auto CreateNewGroupCommandText = "insert into Groups_User(Title, CreatedAt) values(?, ?)";
constexpr auto AddBookToGroupCommandText = "insert into Groups_List_User(ObjectID, GroupID, CreatedAt) values(?, ?, ?)";

}
