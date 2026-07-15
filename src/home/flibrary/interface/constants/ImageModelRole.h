#pragma once

namespace HomeCompa::Flibrary
{

struct ImageModelRole
{
	enum Value
	{
		Folder = Qt::UserRole + 1,
		Ready,
		Prepare,
		ImageSize,
		Last
	};
};

} // namespace HomeCompa::Flibrary
