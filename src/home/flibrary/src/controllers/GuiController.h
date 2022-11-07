#pragma once

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

class QAbstractItemModel;

namespace HomeCompa::Flibrary {

class AnnotationController;
class ModelController;
class BooksModelController;

class GuiController
	: public QObject
{
	NON_COPY_MOVABLE(GuiController)
	Q_OBJECT
	Q_PROPERTY(bool opened READ GetOpened NOTIFY OpenedChanged)
	Q_PROPERTY(QString title READ GetTitle NOTIFY TitleChanged)

public:
	explicit GuiController(QObject * parent = nullptr);
	~GuiController() override;

	void Start();

public:
	Q_INVOKABLE void OnKeyPressed(int key, int modifiers);
	Q_INVOKABLE ModelController * GetNavigationModelControllerAuthors();
	Q_INVOKABLE ModelController * GetNavigationModelControllerSeries();
	Q_INVOKABLE ModelController * GetNavigationModelControllerGenres();
	Q_INVOKABLE BooksModelController * GetBooksModelControllerList();
	Q_INVOKABLE BooksModelController * GetBooksModelControllerTree();
	Q_INVOKABLE BooksModelController * GetBooksModelController();

signals:
	void OpenedChanged() const;
	void TitleChanged() const;

private: // property getters
	bool GetOpened() const noexcept;
	const QString & GetTitle() const noexcept;

private: // property setters

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
