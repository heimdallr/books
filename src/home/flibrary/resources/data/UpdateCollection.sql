CREATE UNIQUE INDEX UIX_Settings_SettingID ON Settings (SettingID);
--@@

CREATE UNIQUE INDEX UIX_Series_SeriesID ON Series (SeriesID);
--@@

CREATE UNIQUE INDEX UIX_Series_SeriesTitle ON Series (SeriesTitle);
--@@

CREATE INDEX IXSeries_SearchSeriesTitle ON Series (SearchSeriesTitle);
--@@

CREATE UNIQUE INDEX UIX_Genres_GenreCode ON Genres (GenreCode);
--@@

CREATE UNIQUE INDEX IXGenres_ParentCode_GenreCode ON Genres (ParentCode, GenreCode);
--@@

CREATE INDEX IXGenres_FB2Code ON Genres (FB2Code);
--@@

CREATE INDEX IXGenres_GenreAlias ON Genres (GenreAlias);
--@@

CREATE UNIQUE INDEX UIX_Authors_AuthorID ON Authors (AuthorID);
--@@

CREATE INDEX IXAuthors_FullName ON Authors (LastName, FirstName, MiddleName);
--@@

CREATE INDEX IXAuthors_SearchName ON Authors (SearchName);
--@@

CREATE UNIQUE INDEX UIX_Books_BookID ON Books (BookID);
--@@

CREATE INDEX IXBooks_SeriesID_SeqNumber ON Books (SeriesID, SeqNumber);
--@@

CREATE INDEX IXBooks_SeriesID_IsDeleted_IsLocal ON Books (SeriesID, IsDeleted, IsLocal);
--@@

CREATE INDEX IXBooks_Title ON Books (Title);
--@@

CREATE INDEX IXBooks_FileName ON Books (FileName);
--@@

CREATE INDEX IXBooks_Folder ON Books (Folder);
--@@

CREATE INDEX IXBooks_IsDeleted ON Books (IsDeleted);
--@@

CREATE INDEX IXBooks_UpdateDate ON Books (UpdateDate);
--@@

CREATE INDEX IXBooks_IsLocal ON Books (IsLocal);
--@@

CREATE INDEX IXBooks_LibID ON Books (LibID);
--@@

CREATE INDEX IXBooks_BookID_IsDeleted_IsLocal ON Books (BookID, IsDeleted, IsLocal);
--@@

CREATE INDEX IXBooks_SearchTitle ON Books (SearchTitle);
--@@

CREATE INDEX IXBooks_SearchLang ON Books (SearchLang);
--@@

CREATE INDEX IXBooks_SearchFolder ON Books (SearchFolder);
--@@

CREATE INDEX IXBooks_SearchFileName ON Books (SearchFileName);
--@@

CREATE INDEX IXBooks_SearchExt ON Books (SearchExt);
--@@

CREATE UNIQUE INDEX UIX_Genre_List_BookID_GenreCode ON Genre_List (BookID, GenreCode);
--@@

CREATE INDEX IXGenreList_GenreCode_BookID ON Genre_List (GenreCode, BookID);
--@@

CREATE UNIQUE INDEX UIX_Author_List_BookID_AuthorID ON Author_List (BookID, AuthorID);
--@@

CREATE INDEX IXAuthorList_AuthorID_BookID ON Author_List (AuthorID, BookID);
--@@

ANALYZE;
--@@
