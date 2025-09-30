#pragma once

namespace HomeCompa
{

/// RAII-класс для временного выставления значения переменной. В деструкторе восстанавливает оригинальное значение.
/// Использование:
/// int var = 42;
/// Core::ValueGuard<int> guard(var, newTemporaryValue);
template <typename T>
class ValueGuard
{
	const T m_oldVal;
	T&      m_val;

public:
	ValueGuard(T& oldVal, const T& newVal = T())
		: m_oldVal(oldVal)
		, m_val(oldVal)
	{
		oldVal = newVal;
	}

	~ValueGuard()
	{
		m_val = m_oldVal;
	}
};

}
