#include "ModelControllers/NavigationSource.h"
#include "ModelControllers/BooksViewType.h"

#include "NavigationSourceProvider.h"

namespace HomeCompa::Flibrary {

namespace {

void Notify(const NavigationSourceProvider & provider)
{
	emit provider.AuthorsVisibleChanged();
	emit provider.SeriesVisibleChanged();
	emit provider.GenresVisibleChanged();
}

}

struct NavigationSourceProvider::Impl
{
	NavigationSource navigationSource { NavigationSource::Undefined };
	BooksViewType booksViewType { BooksViewType::Undefined };
};

NavigationSourceProvider::NavigationSourceProvider(QObject * parent)
	: QObject(parent)
{
}

NavigationSourceProvider::~NavigationSourceProvider() = default;

NavigationSource NavigationSourceProvider::GetSource() const noexcept
{
	return m_impl->navigationSource;
}

void NavigationSourceProvider::SetSource(const NavigationSource source)
{
	m_impl->navigationSource = source;
	Notify(*this);
}

BooksViewType NavigationSourceProvider::GetBookViewType() const noexcept
{
	return m_impl->booksViewType;
}

void NavigationSourceProvider::SetBookViewType(const BooksViewType type)
{
	m_impl->booksViewType = type;
	Notify(*this);
}

bool NavigationSourceProvider::IsAuthorsVisible() const noexcept
{
	return m_impl->navigationSource != NavigationSource::Authors;
}

bool NavigationSourceProvider::IsSeriesVisible() const noexcept
{
	return m_impl->navigationSource != NavigationSource::Series;
}

bool NavigationSourceProvider::IsGenresVisible() const noexcept
{
	return m_impl->navigationSource != NavigationSource::Genres;
}

}
