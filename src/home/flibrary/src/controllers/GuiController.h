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

public:
	explicit GuiController(const std::string & databaseName, QObject * parent = nullptr);
	~GuiController() override;

	void Start();

public:
	Q_INVOKABLE void OnKeyPressed(int key, int modifiers);
	Q_INVOKABLE ModelController * GetNavigationModelControllerAuthors();
	Q_INVOKABLE ModelController * GetNavigationModelControllerSeries();
	Q_INVOKABLE ModelController * GetNavigationModelControllerGenres();
	Q_INVOKABLE ModelController * GetBooksModelController() noexcept;

	Q_INVOKABLE bool IsAuthorsVisible() const noexcept;
	Q_INVOKABLE bool IsSeriesVisible() const noexcept;
	Q_INVOKABLE bool IsGenresVisible() const noexcept;

signals:
	void RunningChanged() const;

private: // property getters

private: // property setters
	bool GetRunning() const noexcept;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
