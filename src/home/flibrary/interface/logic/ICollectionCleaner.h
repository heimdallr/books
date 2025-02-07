#pragma once

#include <QString>

namespace HomeCompa::Flibrary {

class ICollectionCleaner  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	struct Book
	{
		long long id;
		QString folder;
		QString file;
	};
	using Books = std::vector<Book>;

	using Callback = std::function<void(bool result)>;

public:
	virtual ~ICollectionCleaner() = default;
	virtual void Remove(Books books, Callback callback) = 0;
};

}
