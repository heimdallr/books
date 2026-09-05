#pragma once

namespace HomeCompa::Flibrary::Constant::UserData::Groups
{

constexpr auto RootNode  = "Groups";
constexpr auto GroupNode = L"Group";

constexpr auto Book    = "Book";
constexpr auto Author  = L"Author";
constexpr auto Series  = L"Series";
constexpr auto Keyword = L"Keyword";

constexpr auto CreateNewGroupCommandText = "insert into Groups_User(Title, CreatedAt) values(?, ?)";
constexpr auto AddBookToGroupCommandText = "insert into Groups_List_User(ObjectID, GroupID, CreatedAt) values(?, ?, ?)";

}
