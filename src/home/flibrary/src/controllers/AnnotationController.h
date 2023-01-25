#pragma once

#include <filesystem>

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

namespace HomeCompa::DB {
class Database;
}
namespace HomeCompa::Flibrary {

class BooksModelControllerObserver;

class AnnotationController
	: public QObject
{
	NON_COPY_MOVABLE(AnnotationController)
	Q_OBJECT

	Q_PROPERTY(QString annotation READ GetAnnotation NOTIFY AnnotationChanged)
	Q_PROPERTY(bool hasCover READ GetHasCover NOTIFY HasCoverChanged)
	Q_PROPERTY(QString cover READ GetCover NOTIFY CoverChanged)
	Q_PROPERTY(int coverCount READ GetCoverCount NOTIFY CoverCountChanged)

public:
	Q_INVOKABLE void CoverNext();
	Q_INVOKABLE void CoverPrev();
	Q_INVOKABLE long long GetCurrentBookId() const noexcept;

signals:
	void AnnotationChanged() const;
	void CoverChanged() const;
	void CoverCountChanged() const;
	void HasCoverChanged() const;

public:
	AnnotationController(DB::Database & db, std::filesystem::path rootFolder);
	~AnnotationController() override;

public:
	BooksModelControllerObserver * GetBooksModelControllerObserver();

private: // property getters
	QString GetAnnotation() const;
	bool GetHasCover() const;
	const QString & GetCover() const;
	int GetCoverCount() const noexcept;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
