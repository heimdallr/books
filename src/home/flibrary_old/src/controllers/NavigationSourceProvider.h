#pragma once

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

namespace HomeCompa::Flibrary {

enum class BooksViewType;
enum class NavigationSource;
class IBooksModelControllerObserver;

class NavigationSourceProvider
	: public QObject
{
	NON_COPY_MOVABLE(NavigationSourceProvider)
	Q_OBJECT
	Q_PROPERTY(bool authorsVisible READ IsAuthorsVisible NOTIFY AuthorsVisibleChanged)
	Q_PROPERTY(bool seriesVisible READ IsSeriesVisible NOTIFY SeriesVisibleChanged)
	Q_PROPERTY(bool genresVisible READ IsGenresVisible NOTIFY GenresVisibleChanged)

signals:
	void AuthorsVisibleChanged() const;
	void SeriesVisibleChanged() const;
	void GenresVisibleChanged() const;

public:
	explicit NavigationSourceProvider(QObject * parent = nullptr);
	~NavigationSourceProvider() override;

	NavigationSource GetSource() const noexcept;
	void SetSource(NavigationSource source);

	BooksViewType GetBookViewType() const noexcept;
	void SetBookViewType(BooksViewType type);

private: // property getters
	bool IsAuthorsVisible() const noexcept;
	bool IsSeriesVisible() const noexcept;
	bool IsGenresVisible() const noexcept;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
