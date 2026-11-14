#pragma once

#include <functional>

class QIODevice;
class QString;

namespace HomeCompa::Flibrary {

class IUserDataController // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	enum class RestoreMode
	{
		FLibrary,
		MyHomeLib,
	};

public:
	using Callback = std::function<void(bool ok, const QString& errorMessage)>;

public:
	virtual ~IUserDataController()                                                     = default;
	virtual void Backup(Callback callback) const                                       = 0;
	virtual void Restore(Callback callback) const                                      = 0;
	virtual void Backup(const QString& path, Callback callback) const                  = 0;
	virtual void Restore(const QString& path, Callback callback) const                 = 0;
	virtual void Backup(QIODevice& stream, Callback callback) const                    = 0;
	virtual void Restore(QIODevice& stream, RestoreMode mode, Callback callback) const = 0;
};

} // namespace HomeCompa::Flibrary
