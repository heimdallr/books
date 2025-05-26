PRAGMA page_size = 16384;
--@@

PRAGMA journal_mode = OFF;
--@@

DROP TABLE IF EXISTS Settings;
--@@

DROP TABLE IF EXISTS Inpx;
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

DROP TABLE IF EXISTS Series_List;
--@@

DROP TABLE IF EXISTS Folders;
--@@

DROP TABLE IF EXISTS Updates;
--@@

DROP TABLE IF EXISTS Reviews;
--@@

CREATE TABLE Reviews (
  BookID INTEGER      NOT NULL,
  Folder VARCHAR (10) NOT NULL
);
--@@

CREATE TABLE Updates (
  UpdateID    INTEGER NOT NULL,
  UpdateTitle INTEGER NOT NULL,
  ParentID    INTEGER NOT NULL,
  IsDeleted   INTEGER NOT NULL DEFAULT(0)
);
--@@

CREATE TABLE Folders (
  FolderID    INTEGER       NOT NULL,
  FolderTitle VARCHAR (200) NOT NULL COLLATE MHL_SYSTEM_NOCASE,
  IsDeleted   INTEGER       NOT NULL DEFAULT(0)
);
--@@

CREATE TABLE Series (
  SeriesID    INTEGER     NOT NULL,
  SeriesTitle VARCHAR(80) NOT NULL COLLATE MHL_SYSTEM_NOCASE,
  SearchTitle VARCHAR (80)         COLLATE NOCASE,
  IsDeleted   INTEGER     NOT NULL DEFAULT(0)
);
--@@

CREATE TABLE Genres (
  GenreCode  VARCHAR(20) NOT NULL COLLATE NOCASE,
  ParentCode VARCHAR(20)          COLLATE NOCASE,
  FB2Code    VARCHAR(20)          COLLATE NOCASE,
  GenreAlias VARCHAR(50) NOT NULL COLLATE MHL_SYSTEM_NOCASE,
  IsDeleted  INTEGER     NOT NULL DEFAULT(0)
);
--@@

CREATE TABLE Authors (
  AuthorID   INTEGER      NOT NULL,
  LastName   VARCHAR(128) NOT NULL COLLATE MHL_SYSTEM_NOCASE,
  FirstName  VARCHAR(128)          COLLATE MHL_SYSTEM_NOCASE,
  MiddleName VARCHAR(128)          COLLATE MHL_SYSTEM_NOCASE,
  SearchName VARCHAR (128)         COLLATE NOCASE,
  IsDeleted  INTEGER      NOT NULL DEFAULT(0)
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
  Lang             VARCHAR(3)             COLLATE MHL_SYSTEM_NOCASE,
  FolderID         INTEGER       NOT NULL,
  FileName         VARCHAR(170)  NOT NULL COLLATE MHL_SYSTEM_NOCASE,
  InsideNo         INTEGER       NOT NULL,
  Ext              VARCHAR(10)            COLLATE MHL_SYSTEM_NOCASE,
  BookSize         INTEGER,
  UpdateID         INTEGER       NOT NULL                           DEFAULT 0,
  IsDeleted        INTEGER       NOT NULL                           DEFAULT 0,
  SearchTitle      VARCHAR (150)          COLLATE NOCASE
);
--@@

CREATE TABLE Series_List (
    SeriesID  INTEGER NOT NULL,
    BookID    INTEGER NOT NULL,
    SeqNumber INTEGER
);
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
  BookID    INTEGER NOT NULL PRIMARY KEY,
  IsDeleted INTEGER,
  UserRate  INTEGER,
  Lang      VARCHAR (3),
  CreatedAt DATETIME,
  FOREIGN KEY (BookID) REFERENCES Books (BookID) ON DELETE CASCADE
);
--@@

CREATE TABLE Groups_User (
  GroupID   INTEGER       NOT NULL PRIMARY KEY AUTOINCREMENT,
  Title     VARCHAR (150) NOT NULL UNIQUE COLLATE MHL_SYSTEM_NOCASE,
  CreatedAt DATETIME,
  IsDeleted  INTEGER      NOT NULL DEFAULT(0)
);
--@@

CREATE TABLE Groups_List_User (
  GroupID   INTEGER  NOT NULL,
  BookID    INTEGER  NOT NULL,
  CreatedAt DATETIME,
  PRIMARY KEY (GroupID, BookID),
  FOREIGN KEY (GroupID) REFERENCES Groups_User (GroupID) ON DELETE CASCADE,
  FOREIGN KEY (BookID)  REFERENCES Books (BookID) ON DELETE CASCADE
);
--@@

CREATE TABLE Searches_User (
  SearchID    INTEGER       NOT NULL PRIMARY KEY AUTOINCREMENT,
  Title       VARCHAR (150) NOT NULL UNIQUE COLLATE MHL_SYSTEM_NOCASE,
  CreatedAt   DATETIME
);
--@@

CREATE TABLE Keywords (
  KeywordID     INTEGER       NOT NULL,
  KeywordTitle  VARCHAR(150)  NOT NULL COLLATE MHL_SYSTEM_NOCASE,
  SearchTitle   VARCHAR(150)           COLLATE NOCASE,
  IsDeleted     INTEGER       NOT NULL DEFAULT(0)
);
--@@

CREATE TABLE Keyword_List (
  KeywordID INTEGER NOT NULL,
  BookID    INTEGER NOT NULL
);
--@@

CREATE TABLE Export_List_User (
  BookID     INTEGER  NOT NULL,
  ExportType INTEGER  NOT NULL,
  CreatedAt  DATETIME NOT NULL
);
--@@

CREATE TABLE Inpx (
  Folder VARCHAR (200) NOT NULL,
  File   VARCHAR (200) NOT NULL,
  Hash   VARCHAR (50)  NOT NULL
);
--@@

CREATE TABLE Settings (
  SettingID    INTEGER NOT NULL PRIMARY KEY,
  SettingValue BLOB
);
--@@

CREATE VIRTUAL TABLE IF NOT EXISTS Authors_Search USING fts5(LastName, FirstName, MiddleName, content=Authors, content_rowid=AuthorID);
--@@

CREATE VIRTUAL TABLE IF NOT EXISTS Books_Search USING fts5(Title, content=Books, content_rowid=BookID);
--@@

CREATE VIRTUAL TABLE IF NOT EXISTS Series_Search USING fts5(SeriesTitle, content=Series, content_rowid=SeriesID);
--@@
