#pragma once

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

class QAbstractItemModel;

namespace HomeCompa::Flibrary {

class AnnotationController;
class ModelController;

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
	Q_INVOKABLE ModelController * GetBooksModelControllerList();
	Q_INVOKABLE ModelController * GetBooksModelControllerTree();
	Q_INVOKABLE void AddCollection(QString name, QString db, QString folder);

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
