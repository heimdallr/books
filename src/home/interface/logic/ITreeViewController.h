#pragma once

namespace HomeCompa {

enum class TreeViewControllerType
{
	Navigation,
	Books,
};

class ITreeViewController
{
public:
	virtual ~ITreeViewController() = default;
};

class AbstractTreeViewController : virtual public ITreeViewController
{
};

}
