#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "interface/logic/ITreeViewController.h"

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class TreeViewControllerBooks final
	: public AbstractTreeViewController
{
	NON_COPY_MOVABLE(TreeViewControllerBooks)

public:
	explicit TreeViewControllerBooks(std::shared_ptr<ISettings> settings);
	~TreeViewControllerBooks() override;

private: // ITreeViewController
	const char * GetTreeViewName() const noexcept override;
	std::vector<const char *> GetModeNames() const override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
