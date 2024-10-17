PRAGMA page_size = 16384;
--@@

PRAGMA journal_mode = OFF;
--@@

DROP TABLE IF EXISTS Books_User;
--@@

DROP TABLE IF EXISTS Groups_List_User;
--@@

DROP TABLE IF EXISTS Groups_User;
--@@

DROP TABLE IF EXISTS Searches_User;
--@@

DROP TABLE IF EXISTS Keyword_List;
--@@

DROP TABLE IF EXISTS Keywords;
--@@

DROP TABLE IF EXISTS Settings;
--@@

DROP TABLE IF EXISTS Series;
--@@

DROP TABLE IF EXISTS Genres;
--@@

DROP TABLE IF EXISTS Authors;
--@@

DROP TABLE IF EXISTS Books;
--@@

DROP TABLE IF EXISTS Genre_List;
--@@

DROP TABLE IF EXISTS Author_List;
--@@

CREATE TABLE Settings (
  SettingID    INTEGER NOT NULL,
  SettingValue BLOB
);
--@@

CREATE TABLE Series (
  SeriesID          INTEGER     NOT NULL,
  SeriesTitle       VARCHAR(80) NOT NULL COLLATE MHL_SYSTEM_NOCASE
);
--@@

CREATE TABLE Genres (
  GenreCode  VARCHAR(20) NOT NULL COLLATE NOCASE,
  ParentCode VARCHAR(20)          COLLATE NOCASE,
  FB2Code    VARCHAR(20)          COLLATE NOCASE,
  GenreAlias VARCHAR(50) NOT NULL COLLATE MHL_SYSTEM_NOCASE
);
--@@

CREATE TABLE Authors (
  AuthorID   INTEGER      NOT NULL,
  LastName   VARCHAR(128) NOT NULL COLLATE MHL_SYSTEM_NOCASE,
  FirstName  VARCHAR(128)          COLLATE MHL_SYSTEM_NOCASE,
  MiddleName VARCHAR(128)          COLLATE MHL_SYSTEM_NOCASE
);
--@@

CREATE TABLE Books (
  BookID           INTEGER       NOT NULL,
  LibID            VARCHAR(200)  NOT NULL COLLATE MHL_SYSTEM_NOCASE,
  Title            VARCHAR(150)  NOT NULL COLLATE MHL_SYSTEM_NOCASE,
  SeriesID         INTEGER,
  SeqNumber        INTEGER,
  UpdateDate       VARCHAR(23)   NOT NULL,
  LibRate          INTEGER       NOT NULL                           DEFAULT 0,
  Lang             VARCHAR(2)            COLLATE MHL_SYSTEM_NOCASE,
  Folder           VARCHAR(200)          COLLATE MHL_SYSTEM_NOCASE,
  FileName         VARCHAR(170)  NOT NULL COLLATE MHL_SYSTEM_NOCASE,
  InsideNo         INTEGER       NOT NULL,
  Ext              VARCHAR(10)           COLLATE MHL_SYSTEM_NOCASE,
  BookSize         INTEGER,
  IsDeleted        INTEGER       NOT NULL                           DEFAULT 0
);
--@@

CREATE TRIGGER TRBooks_BD BEFORE DELETE ON Books
  BEGIN
    DELETE FROM Genre_List WHERE BookID = OLD.BookID;
    DELETE FROM Author_List WHERE BookID = OLD.BookID;
    DELETE FROM Series WHERE SeriesID IN (SELECT b.SeriesID FROM Books b WHERE  b.SeriesID = OLD.SeriesID GROUP BY b.SeriesID HAVING COUNT(b.SeriesID) <= 1);
  END;
--@@

CREATE TABLE Genre_List (
  GenreCode VARCHAR(20) NOT NULL COLLATE NOCASE,
  BookID    INTEGER     NOT NULL
);
--@@

CREATE TABLE Author_List (
  AuthorID INTEGER NOT NULL,
  BookID   INTEGER NOT NULL
);
--@@

CREATE TABLE Books_User (
    BookID    INTEGER  NOT NULL
                       PRIMARY KEY,
    IsDeleted INTEGER,
    UserRate  INTEGER,
    CreatedAt DATETIME,
    FOREIGN KEY (
        BookID
    )
    REFERENCES Books (BookID)
);
--@@

CREATE TABLE Groups_User (
    GroupID   INTEGER       NOT NULL
                            PRIMARY KEY AUTOINCREMENT,
    Title     VARCHAR (150) NOT NULL
                            UNIQUE
                            COLLATE MHL_SYSTEM_NOCASE,
    CreatedAt DATETIME
);
--@@

CREATE TABLE Groups_List_User (
    GroupID   INTEGER  NOT NULL,
    BookID    INTEGER  NOT NULL,
    CreatedAt DATETIME,
    PRIMARY KEY (
        GroupID,
        BookID
    ),
    FOREIGN KEY (
        GroupID
    )
    REFERENCES Groups_User (GroupID) ON DELETE CASCADE,
    FOREIGN KEY (
        BookID
    )
    REFERENCES Books (BookID)
);
--@@

CREATE TABLE Searches_User (
    SearchID  INTEGER       NOT NULL
                            PRIMARY KEY AUTOINCREMENT,
    Title     VARCHAR (150) NOT NULL
                            UNIQUE
                            COLLATE MHL_SYSTEM_NOCASE,
    CreatedAt DATETIME
);
--@@

CREATE TABLE Keywords (
  KeywordID     INTEGER       NOT NULL,
  KeywordTitle  VARCHAR(150)  NOT NULL COLLATE MHL_SYSTEM_NOCASE
);
--@@

CREATE TABLE Keyword_List (
  KeywordID INTEGER NOT NULL,
  BookID    INTEGER NOT NULL
);
--@@
