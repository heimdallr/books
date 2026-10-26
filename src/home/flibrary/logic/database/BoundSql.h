#pragma once

#include <format>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <QString>

#include "database/interface/IQuery.h"

namespace HomeCompa::Flibrary {

// A piece of SQL text with the values for its named placeholders (":name"), so that request and navigation values reach
// SQLite as bound parameters and never become part of the SQL text. Pieces are glued together with Append() / Prepended()
// and their values travel with them; Bind() binds them all to the query built from the final text.
// Named placeholders only: SQLite numbers a positional "?" after the named ones that come before it.
class BoundSql
{
public:
	// No value (e.g. an id that is not a number) binds NULL, which matches nothing
	using Value = std::variant<std::monostate, long long, std::string>;

	// An integer column value: NULL when the text is not a number
	static Value IntegerValue(const QString& value)
	{
		bool       ok = false;
		const auto id = value.toLongLong(&ok);
		return ok ? Value { id } : Value {};
	}

	// A text column value, e.g. a genre code like "001" that must not be compared as a number
	static Value TextValue(const QString& value)
	{
		return value.toStdString();
	}

public:
	BoundSql() = default;

	explicit BoundSql(QString text)
		: m_text { std::move(text) }
	{
	}

	BoundSql(QString text, std::string placeholder, Value value)
		: m_text { std::move(text) }
	{
		AddValue(std::move(placeholder), std::move(value));
	}

	[[nodiscard]] const QString& Text() const noexcept
	{
		return m_text;
	}

	[[nodiscard]] bool IsEmpty() const noexcept
	{
		return m_text.isEmpty();
	}

	void AddValue(std::string placeholder, Value value)
	{
		m_values.emplace_back(std::move(placeholder), std::move(value));
	}

	// Appends a line of SQL text that uses one more placeholder
	void Add(const QString& text, std::string placeholder, Value value)
	{
		Append(text);
		AddValue(std::move(placeholder), std::move(value));
	}

	void Append(const QString& text)
	{
		m_text = m_text.isEmpty() ? text : QString("%1\n%2").arg(m_text, text);
	}

	// Appends another piece together with its values; a piece may carry only values for placeholders already in this text
	void Append(const BoundSql& other)
	{
		if (!other.IsEmpty())
			Append(other.m_text);
		m_values.insert(m_values.end(), other.m_values.begin(), other.m_values.end());
	}

	[[nodiscard]] BoundSql Prepended(const QString& text) const
	{
		auto result   = *this;
		result.m_text = QString("%1\n%2").arg(text, m_text);
		return result;
	}

	// Binds every value by its placeholder name; throws like ICommand::BindStream when one cannot be bound,
	// which means the query text lacks a placeholder this piece expects
	void Bind(DB::IQuery& query) const
	{
		for (const auto& [placeholder, value] : m_values)
		{
			const auto rc = std::visit(
				[&]<typename T>(const T& v) {
					if constexpr (std::is_same_v<T, std::monostate>)
						return query.Bind(placeholder);
					else if constexpr (std::is_same_v<T, long long>)
						return query.BindLong(placeholder, v);
					else
						return query.BindString(placeholder, std::string_view { v });
				},
				value
			);
			if (rc != 0)
				throw std::runtime_error(std::format("cannot bind {}", placeholder));
		}
	}

private:
	QString m_text;

	// The names are null-terminated strings on purpose: the sqlite wrapper passes name.data() to SQLite
	std::vector<std::pair<std::string, Value>> m_values;
};

} // namespace HomeCompa::Flibrary
