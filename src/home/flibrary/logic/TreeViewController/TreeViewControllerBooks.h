#pragma once

#include "fnd/memory.h"
#include "interface/logic/ITreeViewController.h"

namespace HomeCompa::Flibrary {

class TreeViewControllerBooks : public AbstractTreeViewController
{
public:
	TreeViewControllerBooks();
	~TreeViewControllerBooks() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
