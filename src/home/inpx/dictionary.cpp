#include "dictionary.h"

using namespace HomeCompa::Inpx;

Dictionary::Iterator::Iterator(const DataStorage& dataStorage, const ViewStorage& viewStorage, const DataStorage::const_iterator& dataIt, const ViewStorage::const_iterator& viewIt)
	: m_dataStorage { &dataStorage }
	, m_viewStorage { &viewStorage }
	, m_dataIt { dataIt }
	, m_viewIt { viewIt }
{
	UpdateValue();
}

Dictionary::Iterator::reference Dictionary::Iterator::operator*() const noexcept
{
	return m_value;
}

Dictionary::Iterator::pointer Dictionary::Iterator::operator->() const noexcept
{
	return &(operator*());
}

Dictionary::Iterator& Dictionary::Iterator::operator++() noexcept
{
	if (m_dataIt != m_dataStorage->end())
	{
		++m_dataIt;
		if (m_dataIt == m_dataStorage->end())
			m_viewIt = m_viewStorage->begin();
	}
	else
	{
		++m_viewIt;
	}

	UpdateValue();

	return *this;
}

Dictionary::Iterator Dictionary::Iterator::operator++(int) noexcept
{
	Iterator tmp = *this;
	++(*this);
	return tmp;
}

bool Dictionary::Iterator::operator==(const Iterator& other) const noexcept
{
	return m_dataIt == other.m_dataIt && m_viewIt == other.m_viewIt;
}

bool Dictionary::Iterator::operator==(Sentinel) const noexcept
{
	return m_dataIt == m_dataStorage->end() && m_viewIt == m_viewStorage->end();
}

void Dictionary::Iterator::UpdateValue() const noexcept
{
	if (m_dataIt != m_dataStorage->end())
	{
		m_value.first  = m_dataIt->first;
		m_value.second = m_dataIt->second;
	}
	else if (m_viewIt != m_viewStorage->end())
	{
		m_value.first  = m_viewIt->first;
		m_value.second = m_viewIt->second;
	}
}

std::pair<QStringView, size_t> Dictionary::emplace(QString key, const size_t value)
{
	if (const auto it = m_view.find(key); it != m_view.end())
		return std::make_pair(it->first, it->second);

	const auto it = m_data.try_emplace(std::move(key), value).first;
	return std::make_pair(it->first, it->second);
}

std::pair<QStringView, size_t> Dictionary::emplace(const QStringView key, const size_t value)
{
	if (const auto it = m_data.find(key); it != m_data.end())
		return std::make_pair(it->first, it->second);

	const auto it = m_view.try_emplace(key, value).first;
	return std::make_pair(it->first, it->second);
}

bool Dictionary::contains(const QStringView key) const
{
	return m_view.contains(key) || m_data.contains(key);
}

std::optional<size_t> Dictionary::find(const QStringView key) const
{
	if (const auto it = m_view.find(key); it != m_view.end())
		return it->second;

	if (const auto it = m_data.find(key); it != m_data.end())
		return it->second;

	return std::nullopt;
}

size_t Dictionary::size() const noexcept
{
	return m_data.size() + m_view.size();
}

bool Dictionary::empty() const noexcept
{
	return m_data.empty() && m_view.empty();
}

size_t Dictionary::erase(const QStringView key)
{
	const auto it = m_data.find(key);
	return static_cast<size_t>(it != m_data.end() ? (m_data.erase(it), 1) : 0) + m_view.erase(key);
}

Dictionary::Iterator Dictionary::cbegin() const
{
	return { m_data, m_view, m_data.begin(), m_view.begin() };
}

Dictionary::Iterator::Sentinel Dictionary::cend() const
{
	return {};
}
