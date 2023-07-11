#pragma once

#include "fnd/memory.h"

class QWidget;

namespace HomeCompa {

class IUiFactory
{
public:
	virtual ~IUiFactory() = default;

	virtual std::shared_ptr<QWidget> CreateTreeViewWidget(enum class TreeViewControllerType type) const = 0;

	virtual std::shared_ptr<class AbstractTreeViewController> GetTreeViewController() const = 0;
};

}
