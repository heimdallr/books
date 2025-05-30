#pragma once

namespace HomeCompa
{

template <typename X, typename Y>
class Linear
{
public:
	constexpr Linear(const X x0, const Y y0, const X x1, const Y y1) noexcept
		: m_x0 { x0 }
		, m_x1 { x1 }
		, m_y0 { y0 }
		, m_y1 { y1 }
	{
		assert(m_x1 != m_x0);
	}

	constexpr Linear(const std::pair<X, Y>& p0, const std::pair<X, Y>& p1) noexcept
		: Linear(p0.first, p0.second, p1.first, p1.second)
	{
	}

	constexpr Y operator()(const X x) const noexcept
	{
		return m_y0 + (x - m_x0) * (m_y1 - m_y0) / (m_x1 - m_x0);
	}

private:
	const X m_x0, m_x1;
	const Y m_y0, m_y1;
};

} // namespace HomeCompa
