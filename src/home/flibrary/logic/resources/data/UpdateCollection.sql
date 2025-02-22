CREATE UNIQUE INDEX UIX_Folders_PrimaryKey ON Folders (FolderID);
--@@

CREATE INDEX IX_Folders_FolderTitle ON Folders(FolderTitle COLLATE NOCASE);
--@@

CREATE UNIQUE INDEX UIX_Series_PrimaryKey ON Series (SeriesID);
--@@

CREATE INDEX IX_Series_SearchTitle ON Series(SearchTitle COLLATE NOCASE);
--@@

CREATE UNIQUE INDEX UIX_GenresPrimaryKey ON Genres (GenreCode);
--@@

CREATE UNIQUE INDEX IX_Genres_ParentCode_GenreCode ON Genres (ParentCode, GenreCode);
--@@

CREATE UNIQUE INDEX UIX_Authors_PrimaryKey ON Authors (AuthorID);
--@@

CREATE INDEX IX_Authors_SearchName ON Authors(SearchName COLLATE NOCASE);
--@@

CREATE UNIQUE INDEX UIX_Books_PrimaryKey ON Books (BookID);
--@@

CREATE INDEX IX_Books_SeriesID_SeqNumber ON Books (SeriesID, SeqNumber);
--@@

CREATE INDEX IX_Books_FolderID ON Books(FolderID);
--@@

CREATE INDEX IX_Book_SearchTitle ON Books(SearchTitle COLLATE NOCASE);
--@@

CREATE INDEX IX_Books_Lang ON Books(Lang);
--@@

CREATE UNIQUE INDEX UIX_Genre_List_PrimaryKey ON Genre_List (BookID, GenreCode);
--@@

CREATE INDEX IX_GenreList_GenreCode_BookID ON Genre_List (GenreCode, BookID);
--@@

CREATE UNIQUE INDEX UIX_Author_List_PrimaryKey ON Author_List (BookID, AuthorID);
--@@

CREATE INDEX IX_AuthorList_AuthorID_BookID ON Author_List (AuthorID, BookID);
--@@

CREATE UNIQUE INDEX UIX_Groups_List_User_PrimaryKey ON Groups_List_User (GroupID, BookID);
--@@

CREATE UNIQUE INDEX UIX_Keywords_PrimaryKey ON Keywords (KeywordID);
--@@

CREATE UNIQUE INDEX UIX_Keyword_List_PrimaryKey ON Keyword_List (KeywordID, BookID);
--@@

CREATE INDEX IX_Keywords_SearchTitle ON Keywords(SearchTitle COLLATE NOCASE);
--@@

CREATE INDEX IX_ExportListUser_BookID ON Export_List_User (BookID);
--@@

ANALYZE;
--@@
