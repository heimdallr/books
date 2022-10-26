#pragma once

#include <string>

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

class QAbstractItemModel;

namespace HomeCompa::Flibrary {

class ModelController;

class GuiController
	: public QObject
{
	NON_COPY_MOVABLE(GuiController)
	Q_OBJECT
	Q_PROPERTY(bool running READ GetRunning NOTIFY RunningChanged)
	Q_PROPERTY(bool opened READ GetOpened NOTIFY OpenedChanged)
	Q_PROPERTY(bool authorsVisible READ IsAuthorsVisible NOTIFY AuthorsVisibleChanged)
	Q_PROPERTY(bool seriesVisible READ IsSeriesVisible NOTIFY SeriesVisibleChanged)
	Q_PROPERTY(bool genresVisible READ IsGenresVisible NOTIFY GenresVisibleChanged)

public:
	explicit GuiController(const std::string & databaseName, QObject * parent = nullptr);
	~GuiController() override;

	void Start();

public:
	Q_INVOKABLE void OnKeyPressed(int key, int modifiers);
	Q_INVOKABLE ModelController * GetNavigationModelControllerAuthors();
	Q_INVOKABLE ModelController * GetNavigationModelControllerSeries();
	Q_INVOKABLE ModelController * GetNavigationModelControllerGenres();
	Q_INVOKABLE ModelController * GetBooksModelControllerList();
	Q_INVOKABLE ModelController * GetBooksModelControllerTree();
	Q_INVOKABLE void AddCollection(const QString & name, const QString & db, const QString & folder);
	Q_INVOKABLE QString SelectFile(const QString & fileName) const;
	Q_INVOKABLE QString SelectFolder(const QString & folderName) const;

signals:
	void RunningChanged() const;
	void AuthorsVisibleChanged() const;
	void SeriesVisibleChanged() const;
	void GenresVisibleChanged() const;
	void OpenedChanged() const;

private: // property getters
	bool IsAuthorsVisible() const noexcept;
	bool IsSeriesVisible() const noexcept;
	bool IsGenresVisible() const noexcept;
	bool GetOpened() const noexcept;

private: // property setters
	bool GetRunning() const noexcept;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
