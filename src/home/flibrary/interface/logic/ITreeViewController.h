#pragma once

#include <vector>

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
	virtual const char * GetTreeViewName() const noexcept = 0;
	virtual std::vector<const char *> GetModeNames() const = 0;
};

class AbstractTreeViewController : virtual public ITreeViewController
{
};

}
