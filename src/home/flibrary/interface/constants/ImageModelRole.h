#pragma once

namespace HomeCompa::Flibrary
{

struct ImageModelRole
{
	enum Value
	{
		BooksRoot = Qt::UserRole + 1,
		Ready,
		Prepare,
		ImageSize,
		Image,
		Filter,
		Author,
		Title,
		Last
	};
};

} // namespace HomeCompa::Flibrary
