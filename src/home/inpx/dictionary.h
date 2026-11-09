#pragma once

#include <unordered_map>

#include <QStringView>

#include "util/StrUtil.h"

namespace HomeCompa::Inpx {

class Dictionary
{
	using DataStorage = std::unordered_map<QString, size_t, Util::WStringHash, Util::WStringEqualTo>;
	using ViewStorage = std::unordered_map<QStringView, size_t, Util::WStringHash, Util::WStringEqualTo>;

public:
	using value_type = std::pair<QStringView, size_t>;

public:
	class Iterator
	{
	public:
		using iterator_concept  = std::forward_iterator_tag;
		using iterator_category = std::forward_iterator_tag;

		using value_type      = Dictionary::value_type;
		using difference_type = std::ptrdiff_t;
		using pointer         = Dictionary::value_type*;
		using reference       = Dictionary::value_type&;

		struct Sentinel
		{
		};

		Iterator() = default;
		Iterator(const DataStorage& dataStorage, const ViewStorage& viewStorage, const DataStorage::const_iterator& dataIt, const ViewStorage::const_iterator& viewIt);

		reference operator*() const noexcept;
		pointer   operator->() const noexcept;

		Iterator& operator++() noexcept;
		Iterator  operator++(int) noexcept;

		bool operator==(const Iterator& other) const noexcept;
		bool operator==(Sentinel) const noexcept;

	private:
		void UpdateValue() const noexcept;

	private:
		const DataStorage* m_dataStorage { nullptr };
		const ViewStorage* m_viewStorage { nullptr };

		DataStorage::const_iterator m_dataIt;
		ViewStorage::const_iterator m_viewIt;

		mutable value_type m_value;
	};

	static_assert(std::forward_iterator<Iterator>);

public:
	std::pair<QStringView, size_t> emplace(QString key, size_t value);
	std::pair<QStringView, size_t> emplace(QStringView key, size_t value);

	[[nodiscard]] bool                  contains(QStringView key) const;
	[[nodiscard]] std::optional<size_t> find(QStringView key) const;

	[[nodiscard]] size_t size() const noexcept;
	[[nodiscard]] bool   empty() const noexcept;
	size_t               erase(QStringView key);

	[[nodiscard]] Iterator           cbegin() const;
	[[nodiscard]] Iterator::Sentinel cend() const;

	template <typename F>
	[[nodiscard]] std::optional<size_t> find_if(const F& f) const
	{
		if (const auto it = std::ranges::find_if(m_data, f); it != m_data.end())
			return it->second;

		if (const auto it = std::ranges::find_if(m_view, f); it != m_view.end())
			return it->second;

		return std::nullopt;
	}

private:
	DataStorage m_data;
	ViewStorage m_view;
};

inline Dictionary::Iterator begin(const Dictionary& dictionary)
{
	return dictionary.cbegin();
}

inline Dictionary::Iterator::Sentinel end(const Dictionary& dictionary)
{
	return dictionary.cend();
}

} // namespace HomeCompa::Inpx
