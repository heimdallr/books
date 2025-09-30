#pragma once
#include <functional>

namespace HomeCompa
{

class ScopedCall
{
public:
	using Function = std::function<void()>;

	ScopedCall()                      = default;
	ScopedCall(const ScopedCall& rhs) = delete;

	ScopedCall(ScopedCall&& rhs) noexcept
		: m_is_initialized(rhs.m_is_initialized)
		, m_init(std::move(rhs.m_init))
		, m_deinit(std::move(rhs.m_deinit))
	{
		rhs.m_is_initialized = false;
	}

	ScopedCall(Function enter, Function leave)
		: m_init(std::move(enter))
		, m_deinit(std::move(leave))
	{
		ScopeEnter();
	}

	explicit ScopedCall(Function leave)
		: m_is_initialized(true)
		, m_deinit(std::move(leave))
	{
	}

	~ScopedCall()
	{
		ScopeLeave();
	}

	ScopedCall& operator=(const ScopedCall& rhs) = delete;

	ScopedCall& operator=(ScopedCall&& rhs) noexcept
	{
		ScopeLeave();

		m_is_initialized     = rhs.m_is_initialized;
		m_init               = std::move(rhs.m_init);
		m_deinit             = std::move(rhs.m_deinit);
		rhs.m_is_initialized = false;

		return *this;
	}

private:
	void ScopeEnter()
	{
		if (!m_is_initialized && static_cast<bool>(m_init))
			m_init();

		m_is_initialized = true;
	}

	void ScopeLeave()
	{
		if (m_is_initialized && static_cast<bool>(m_deinit))
			m_deinit();

		m_is_initialized = false;
	}

private:
	bool     m_is_initialized { false };
	Function m_init;
	Function m_deinit;
};

} // namespace HomeCompa
