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
		Save,
		SaveStop,
		Filter,
		Author,
		Title,
		FileName,
		Last
	};
};

} // namespace HomeCompa::Flibrary
