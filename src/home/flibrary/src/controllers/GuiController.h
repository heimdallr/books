#pragma once

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

class QAbstractItemModel;

namespace HomeCompa::Flibrary {

class AnnotationController;
class BooksModelController;
class ModelController;
class ComboBoxController;

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
	Q_INVOKABLE ModelController * GetNavigationModelControllerAuthors();
	Q_INVOKABLE ModelController * GetNavigationModelControllerSeries();
	Q_INVOKABLE ModelController * GetNavigationModelControllerGenres();
	Q_INVOKABLE BooksModelController * GetBooksModelControllerList();
	Q_INVOKABLE BooksModelController * GetBooksModelControllerTree();
	Q_INVOKABLE BooksModelController * GetBooksModelController();
	Q_INVOKABLE static int GetPixelMetric(const QVariant & metric);
	Q_INVOKABLE ComboBoxController * GetViewSourceComboBoxNavigationController() noexcept;
	Q_INVOKABLE ComboBoxController * GetViewSourceComboBoxBooksController() noexcept;
	Q_INVOKABLE ComboBoxController * GetLanguageComboBoxBooksController() noexcept;

signals:
	void OpenedChanged() const;
	void TitleChanged() const;

private:
	bool eventFilter(QObject * obj, QEvent * event) override;

private: // property getters
	bool GetOpened() const noexcept;
	QString GetTitle() const;

private: // property setters

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
