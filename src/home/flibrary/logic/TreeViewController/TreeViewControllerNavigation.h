#pragma once

#include "fnd/memory.h"
#include "interface/logic/ITreeViewController.h"

namespace HomeCompa::Flibrary {

class TreeViewControllerNavigation : public AbstractTreeViewController
{
public:
	TreeViewControllerNavigation();
	~TreeViewControllerNavigation() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
