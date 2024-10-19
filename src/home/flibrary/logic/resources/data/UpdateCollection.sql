CREATE UNIQUE INDEX UIX_Series_PrimaryKey ON Series (SeriesID);
--@@

CREATE UNIQUE INDEX UIX_GenresPrimaryKey ON Genres (GenreCode);
--@@

CREATE UNIQUE INDEX IX_Genres_ParentCode_GenreCode ON Genres (ParentCode, GenreCode);
--@@

CREATE UNIQUE INDEX UIX_Authors_PrimaryKey ON Authors (AuthorID);
--@@

CREATE UNIQUE INDEX UIX_Books_PrimaryKey ON Books (BookID);
--@@

CREATE INDEX IX_Books_SeriesID_SeqNumber ON Books (SeriesID, SeqNumber);
--@@

CREATE INDEX IX_Books_Folder ON Books (Folder);
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

CREATE INDEX IX_ExportListUser_BookID ON Export_List_User (BookID);
--@@

ANALYZE;
--@@
