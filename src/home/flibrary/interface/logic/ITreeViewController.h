#pragma once

#include <vector>

namespace HomeCompa::Flibrary {

enum class TreeViewControllerType
{
	Navigation,
	Books,
};

class ITreeViewController  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ITreeViewController() = default;
	[[nodiscard]] virtual std::vector<const char *> GetModeNames() const = 0;
	virtual void SetModeIndex(int index) = 0;
};

class AbstractTreeViewController : virtual public ITreeViewController
{
};

}
