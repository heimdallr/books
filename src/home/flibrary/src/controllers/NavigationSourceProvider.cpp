
#include "ModelControllers/NavigationSource.h"

#include "NavigationSourceProvider.h"

namespace HomeCompa::Flibrary {

struct NavigationSourceProvider::Impl
{
	NavigationSource navigationSource { NavigationSource::Undefined };
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

void NavigationSourceProvider::SetSource(const NavigationSource source) noexcept
{
	m_impl->navigationSource = source;
	emit AuthorsVisibleChanged();
	emit SeriesVisibleChanged();
	emit GenresVisibleChanged();
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
