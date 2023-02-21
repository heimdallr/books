#pragma once

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

class QAbstractItemModel;

namespace HomeCompa::Flibrary {

class AnnotationController;
class BooksModelController;
class CollectionController;
class ComboBoxController;
class GroupsModelController;
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
	Q_INVOKABLE AnnotationController * GetAnnotationController();
	Q_INVOKABLE GroupsModelController * GetGroupsModelController();
	Q_INVOKABLE ModelController * GetNavigationModelController();
	Q_INVOKABLE BooksModelController * GetBooksModelController();
	Q_INVOKABLE BooksModelController * GetCurrentBooksModelController();
	Q_INVOKABLE ComboBoxController * GetViewSourceComboBoxNavigationController() noexcept;
	Q_INVOKABLE ComboBoxController * GetViewSourceComboBoxBooksController() noexcept;
	Q_INVOKABLE ComboBoxController * GetLanguageComboBoxBooksController() noexcept;
	Q_INVOKABLE CollectionController * GetCollectionController();
	Q_INVOKABLE void LogCollectionStatistics();
	Q_INVOKABLE void HandleLink(const QString & link, long long bookId);
	Q_INVOKABLE void BackupUserData();
	Q_INVOKABLE void RestoreUserData();
	Q_INVOKABLE static int GetPixelMetric(const QVariant & metric);

signals:
	void OpenedChanged() const;
	void TitleChanged() const;

	void ShowLog(bool) const;

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
