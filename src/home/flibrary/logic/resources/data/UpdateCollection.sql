CREATE UNIQUE INDEX UIX_Folders_PrimaryKey ON Folders (FolderID);
--@@

CREATE INDEX IX_Folders_FolderTitle ON Folders(FolderTitle COLLATE NOCASE);
--@@

CREATE UNIQUE INDEX UIX_Series_PrimaryKey ON Series (SeriesID);
--@@

CREATE UNIQUE INDEX UIX_Series_List_PrimaryKey ON Series_List (SeriesID, BookID);
--@@

CREATE INDEX IX_Series_List_BookID_SeriesID ON Series_List (BookID, SeriesID);
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

CREATE INDEX IX_Books_UpdateID ON Books(UpdateID);
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

CREATE INDEX IX_Keyword_List_BookID_KeywordID ON Keyword_List (BookID, KeywordID);
--@@

CREATE INDEX IX_Keywords_SearchTitle ON Keywords(SearchTitle COLLATE NOCASE);
--@@

CREATE INDEX IX_ExportListUser_BookID ON Export_List_User (BookID);
--@@

CREATE UNIQUE INDEX UIX_Inpx_PrimaryKey ON Inpx (Folder COLLATE NOCASE, File COLLATE NOCASE);
--@@

CREATE UNIQUE INDEX UIX_Update_PrimaryKey ON Updates (UpdateID);
--@@

CREATE INDEX IX_Update_ParentID ON Updates (ParentID);
--@@

CREATE UNIQUE INDEX UIX_Reviews_PrimaryKey ON Reviews (BookID, Folder);
--@@

CREATE VIEW Books_View (
    BookID,
    LibID,
    Title,
    SeriesID,
    SeqNumber,
    UpdateDate,
    LibRate,
    Lang,
    FolderID,
    FolderTitle,
    FileName,
    BookSize,
    UpdateID,
    UpdateTitle,
    IsDeleted,
    UserRate
)
AS
    SELECT b.BookID,
           b.LibID,
           b.Title,
           b.SeriesID,
           b.SeqNumber,
           b.UpdateDate,
           b.LibRate,
           b.Lang,
           b.FolderID,
           f.FolderTitle,
           b.FileName || b.Ext AS FileName,
           b.BookSize,
           b.UpdateID,
           u.UpdateTitle,
           coalesce(bu.IsDeleted, b.IsDeleted) AS IsDeleted,
           bu.UserRate
      FROM Books b
           JOIN
           Folders f ON f.FolderID = b.FolderID
           JOIN
           Updates u ON u.UpdateID = b.UpdateID
           LEFT JOIN
           Books_User bu ON bu.BookID = b.BookID;
--@@
