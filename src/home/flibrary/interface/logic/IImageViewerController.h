#pragma once

class QAbstractItemModel;
class QString;

namespace HomeCompa::Flibrary
{

class IImageViewerController // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IImageViewerController() = default;

	virtual QAbstractItemModel* GetImageModel() noexcept            = 0;
	virtual void                SetImageSize(int value)             = 0;
};

} // namespace HomeCompa::Flibrary
