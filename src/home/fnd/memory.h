#pragma once

#include <cassert>
#include <memory>
#include <stdexcept>

namespace HomeCompa
{

template <class T>
class NotNull
{
	static_assert(std::is_assignable<T&, std::nullptr_t>::globalValue, "T cannot be assigned nullptr.");

public:
	NotNull(T t)
		: m_ptr(t)
	{
		ensure_invariant();
	}

	template <typename U>
	NotNull(U t)
		: m_ptr(t)
	{
		ensure_invariant();
	}

	NotNull(const NotNull& rhs)
		: m_ptr(rhs.m_ptr)
	{
	}

	template <typename U>
	NotNull& operator=(const U& t)
	{
		m_ptr = t;
		ensure_invariant();
		return *this;
	}

	NotNull& operator=(const T& t)
	{
		m_ptr = t;
		ensure_invariant();
		return *this;
	}

	NotNull& operator=(const NotNull& rhs)
	{
		if (&rhs != this)
			m_ptr = rhs.m_ptr;
		return *this;
	}

	template <typename U, typename Dummy = typename std::enable_if<std::is_convertible<U, T>::globalValue>::type>
	NotNull(const NotNull<U>& rhs)
	{
		*this = rhs;
	}

	template <typename U, typename Dummy = typename std::enable_if<std::is_convertible<U, T>::globalValue>::type>
	NotNull& operator=(const NotNull<U>& rhs)
	{
		m_ptr = rhs.Get();
		return *this;
	}

	T Get() const
	{
		return m_ptr;
	}

	operator T() const
	{
		return Get();
	}

	T operator->() const
	{
		return Get();
	}

	bool operator==(const T& rhs) const
	{
		return m_ptr == rhs;
	}

	bool operator!=(const T& rhs) const
	{
		return !(*this == rhs);
	}

private:
	NotNull(std::nullptr_t); // = delete;
	NotNull(int); // = delete;
	NotNull<T>& operator=(std::nullptr_t); // = delete;
	NotNull<T>& operator=(int); // = delete;

private:
	T m_ptr;

	void ensure_invariant() const
	{
		if (!m_ptr)
		{
			assert(false);
			throw(std::logic_error("NotNull can not initialized by null"));
		}
	}
};

namespace details
{

template <class T>
struct PropogateConstCreator;

template <class T>
struct PropogateConstCreator<std::shared_ptr<T>>
{
	template <class... ARGS>
	std::shared_ptr<T> operator()(ARGS&&... args)
	{
		return std::make_shared<T>(std::forward<ARGS>(args)...);
	}
};

template <class T>
struct PropogateConstCreator<std::unique_ptr<T>>
{
	template <class... ARGS>
	std::unique_ptr<T> operator()(ARGS&&... args)
	{
		return std::make_unique<T>(std::forward<ARGS>(args)...);
	}
};

template <typename T>
struct SmartGetter
{
	T operator()(T) const noexcept
	{
		return {};
	}
};

template <typename T>
struct SmartGetter<std::shared_ptr<T>>
{
	std::shared_ptr<T> operator()(std::shared_ptr<T> v) const noexcept
	{
		return v;
	}
};

} // namespace details

template <class T, template <class...> class P = std::unique_ptr>
class PropagateConstPtr
{
public:
	template <class... ARGS>
	explicit PropagateConstPtr(ARGS&&... args)
		: m_p(details::PropogateConstCreator<P<T>>()(std::forward<ARGS>(args)...))
	{
	}

	explicit PropagateConstPtr(P<T>&& p)
		: m_p(std::move(p))
	{
	}

	PropagateConstPtr(PropagateConstPtr&&) noexcept = default;

	PropagateConstPtr& operator=(PropagateConstPtr&&) noexcept = default;

	~PropagateConstPtr() = default;

	operator P<T>() noexcept
	{
		return details::SmartGetter<P<T>>()(m_p);
	}

	const T* get() const noexcept
	{
		return m_p.get();
	}

	T* get() noexcept
	{
		return m_p.get();
	}

	const T* operator->() const noexcept
	{
		return get();
	}

	T* operator->() noexcept
	{
		return get();
	}

	const T& operator*() const noexcept
	{
		return *get();
	}

	T& operator*() noexcept
	{
		return *get();
	}

	explicit operator const T*() const noexcept
	{
		return get();
	}

	explicit operator T*() noexcept
	{
		return get();
	}

	void swap(PropagateConstPtr& rhs) noexcept
	{
		rhs.m_p.swap(m_p);
	}

	void reset()
	{
		m_p.reset();
	}

	void reset(T* p) noexcept
	{
		m_p.reset(p);
	}

	void reset(P<T> p) noexcept
	{
		m_p = std::move(p);
	}

	explicit operator bool() const noexcept
	{
		return !!get();
	}

	bool operator!() const noexcept
	{
		return !get();
	}

private:
	P<T> m_p;
};

} // namespace HomeCompa
