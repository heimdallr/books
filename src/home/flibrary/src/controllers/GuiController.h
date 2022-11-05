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
	Q_PROPERTY(bool running READ GetRunning NOTIFY RunningChanged)
	Q_PROPERTY(bool opened READ GetOpened NOTIFY OpenedChanged)
	Q_PROPERTY(bool authorsVisible READ IsAuthorsVisible NOTIFY AuthorsVisibleChanged)
	Q_PROPERTY(bool seriesVisible READ IsSeriesVisible NOTIFY SeriesVisibleChanged)
	Q_PROPERTY(bool genresVisible READ IsGenresVisible NOTIFY GenresVisibleChanged)
	Q_PROPERTY(QStringList languages READ GetLanguages NOTIFY LanguagesChanged)
	Q_PROPERTY(QStringList locales READ GetLocales CONSTANT)
	Q_PROPERTY(QString language READ GetLanguage WRITE SetLanguage)
	Q_PROPERTY(QString locale READ GetLocale WRITE SetLocale NOTIFY LocaleChanged)
	Q_PROPERTY(QString title READ GetTitle NOTIFY TitleChanged)

public:
	explicit GuiController(QObject * parent = nullptr);
	~GuiController() override;

	void Start();

public:
	Q_INVOKABLE void OnKeyPressed(int key, int modifiers);
	Q_INVOKABLE AnnotationController * GetAnnotationController();
	Q_INVOKABLE ModelController * GetNavigationModelControllerAuthors();
	Q_INVOKABLE ModelController * GetNavigationModelControllerSeries();
	Q_INVOKABLE ModelController * GetNavigationModelControllerGenres();
	Q_INVOKABLE ModelController * GetBooksModelControllerList();
	Q_INVOKABLE ModelController * GetBooksModelControllerTree();
	Q_INVOKABLE void AddCollection(QString name, QString db, QString folder);
	Q_INVOKABLE QString SelectFile(const QString & fileName) const;
	Q_INVOKABLE QString SelectFolder(const QString & folderName) const;
	Q_INVOKABLE void Restart();

signals:
	void RunningChanged() const;
	void AuthorsVisibleChanged() const;
	void SeriesVisibleChanged() const;
	void GenresVisibleChanged() const;
	void OpenedChanged() const;
	void LanguagesChanged() const;
	void LocaleChanged() const;
	void TitleChanged() const;

private: // property getters
	bool IsAuthorsVisible() const noexcept;
	bool IsSeriesVisible() const noexcept;
	bool IsGenresVisible() const noexcept;
	bool GetOpened() const noexcept;
	bool GetRunning() const noexcept;
	QString GetLanguage();
	QString GetLocale() const;
	const QString & GetTitle() const noexcept;
	QStringList GetLanguages();
	QStringList GetLocales() const;

private: // property setters
	void SetLanguage(const QString & language);
	void SetLocale(const QString & locale);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
