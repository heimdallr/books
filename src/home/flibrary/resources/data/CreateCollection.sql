-------------------------------------------------------------------------------
--
-- MyHomeLib
--
-- Copyright (C) 2008-2010 Aleksey Penkov
--
-- Author(s)           eg
--                     Nick Rymanov    nrymanov@gmail.com
-- Created             04.09.2010
-- Description
--
-- $Id: CreateCollectionDB_SQLite.sql 1064 2011-09-02 11:33:04Z eg_ $
--
-- History
--
-- Notes
--   При изменении схемы базы данных необходимо изменить значение константы TBookCollection_SQLite.DATABASE_VERSION
-------------------------------------------------------------------------------

PRAGMA page_size = 16384;
PRAGMA journal_mode = OFF;
--@@

DROP TABLE IF EXISTS Books_User;
--@@

DROP TABLE IF EXISTS Groups_List_User;
--@@

DROP TABLE IF EXISTS Groups_User;
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
  SeriesTitle       VARCHAR(80) NOT NULL COLLATE MHL_SYSTEM_NOCASE,
  SearchSeriesTitle VARCHAR(80)          COLLATE NOCASE
);
--@@

CREATE TRIGGER TRSeries_AI AFTER INSERT ON Series WHEN MHL_TRIGGERS_ON()
  BEGIN
    UPDATE Series
    SET SearchSeriesTitle = MHL_UPPER(NEW.SeriesTitle)
    WHERE rowid = NEW.rowid;
  END;
--@@

CREATE TRIGGER TRSeries_AU AFTER UPDATE ON Series WHEN MHL_TRIGGERS_ON()
  BEGIN
    UPDATE Series
    SET SearchSeriesTitle = MHL_UPPER(NEW.SeriesTitle)
    WHERE rowid = NEW.rowid;
  END;
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
  MiddleName VARCHAR(128)          COLLATE MHL_SYSTEM_NOCASE,
  SearchName VARCHAR(512)          COLLATE NOCASE
);
--@@

CREATE TRIGGER TRAuthors_AI AFTER INSERT ON Authors WHEN MHL_TRIGGERS_ON()
  BEGIN
    UPDATE Authors
    SET SearchName = MHL_FULLNAME(NEW.LastName, NEW.FirstName, NEW.MiddleName, 1)
    WHERE rowid = NEW.rowid;
  END;
--@@

CREATE TRIGGER TRAuthors_AU AFTER UPDATE ON Authors WHEN MHL_TRIGGERS_ON()
  BEGIN
    UPDATE Authors
    SET SearchName = MHL_FULLNAME(NEW.LastName, NEW.FirstName, NEW.MiddleName, 1)
    WHERE rowid = NEW.rowid;
  END;
--@@

CREATE TABLE Books (
  BookID           INTEGER       NOT NULL,
  LibID            VARCHAR(2048) NOT NULL COLLATE MHL_SYSTEM_NOCASE,
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
  IsLocal          INTEGER       NOT NULL                           DEFAULT 0,
  IsDeleted        INTEGER       NOT NULL                           DEFAULT 0,
  KeyWords         VARCHAR(255)          COLLATE MHL_SYSTEM_NOCASE,
  Rate             INTEGER       NOT NULL                           DEFAULT 0,
  Progress         INTEGER       NOT NULL                           DEFAULT 0,
  Annotation       VARCHAR(4096)         COLLATE MHL_SYSTEM_NOCASE,
  Review           BLOB,
  SearchTitle      VARCHAR(150)          COLLATE NOCASE,
  SearchLang       VARCHAR(2)            COLLATE NOCASE,
  SearchFolder     VARCHAR(200)          COLLATE NOCASE,
  SearchFileName   VARCHAR(170)          COLLATE NOCASE,
  SearchExt        VARCHAR(10)           COLLATE NOCASE,
  SearchKeyWords   VARCHAR(255)          COLLATE NOCASE,
  SearchAnnotation VARCHAR(4096)         COLLATE NOCASE
);
--@@

CREATE TRIGGER TRBooks_AI AFTER INSERT ON Books WHEN MHL_TRIGGERS_ON()
  BEGIN
    UPDATE Books
    SET
      SearchTitle      = MHL_UPPER(NEW.Title),
      SearchLang       = MHL_UPPER(NEW.Lang),
      SearchFolder     = MHL_UPPER(NEW.Folder),
      SearchFileName   = MHL_UPPER(NEW.FileName),
      SearchExt        = MHL_UPPER(NEW.Ext),
      SearchKeyWords   = MHL_UPPER(NEW.KeyWords),
      SearchAnnotation = MHL_UPPER(NEW.Annotation)
    WHERE
      rowid = NEW.rowid;
  END;
--@@

CREATE TRIGGER TRBooks_AU AFTER UPDATE OF Title, Lang, Folder, FileName, Ext, KeyWords, Annotation ON Books WHEN MHL_TRIGGERS_ON()
  BEGIN
    UPDATE Books
    SET
      SearchTitle      = MHL_UPPER(NEW.Title),
      SearchLang       = MHL_UPPER(NEW.Lang),
      SearchFolder     = MHL_UPPER(NEW.Folder),
      SearchFileName   = MHL_UPPER(NEW.FileName),
      SearchExt        = MHL_UPPER(NEW.Ext),
      SearchKeyWords   = MHL_UPPER(NEW.KeyWords),
      SearchAnnotation = MHL_UPPER(NEW.Annotation)
    WHERE
      rowid = NEW.rowid;
  END;
--@@

-- CREATE TRIGGER TRBooks_BD BEFORE DELETE ON Books
--  BEGIN
--    DELETE FROM Genre_List WHERE BookID = OLD.BookID;
--    DELETE FROM Author_List WHERE BookID = OLD.BookID;
--    DELETE FROM Series WHERE SeriesID IN (SELECT b.SeriesID FROM Books b WHERE  b.SeriesID = OLD.SeriesID GROUP BY b.SeriesID HAVING COUNT(b.SeriesID) <= 1);
--    DELETE FROM Authors WHERE NOT AuthorID in (SELECT DISTINCT al.AuthorID FROM Author_List al);
--  END;
--@@

CREATE TRIGGER TRBooks_BD BEFORE DELETE ON Books
  BEGIN
    DELETE FROM Genre_List WHERE BookID = OLD.BookID;
--	  DELETE FROM Authors WHERE AuthorID in (SELECT DISTINCT AuthorID FROM Author_List WHERE BookID = OLD.BookID);
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
