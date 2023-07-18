#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "AbstractTreeViewController.h"

class QString;

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class TreeViewControllerBooks final
	: public AbstractTreeViewController
{
	NON_COPY_MOVABLE(TreeViewControllerBooks)

public:
	explicit TreeViewControllerBooks(std::shared_ptr<ISettings> settings
		, std::shared_ptr<DataProvider> dataProvider
		, std::shared_ptr<AbstractModelProvider> modelProvider
	);
	~TreeViewControllerBooks() override;

private: // ITreeViewController
	[[nodiscard]] std::vector<const char *> GetModeNames() const override;
	void SetModeIndex(int index) override;

private: // AbstractTreeViewController
	void OnModeChanged(const QVariant & mode) override;
	[[nodiscard]] int GetModeIndex(const QVariant & mode) const override;
	[[nodiscard]] ItemType GetItemType() const noexcept override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
