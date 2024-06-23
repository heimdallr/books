#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/ui/ITreeViewDelegate.h"

class QAbstractScrollArea;
class QString;
class QVariant;

namespace HomeCompa::Flibrary {

class TreeViewDelegateNavigation final : public ITreeViewDelegate
{
	NON_COPY_MOVABLE(TreeViewDelegateNavigation)

public:
	using TextDelegate = QString(*)(const QVariant & value);

public:
	explicit TreeViewDelegateNavigation(const std::shared_ptr<const class IUiFactory> & uiFactory);
	~TreeViewDelegateNavigation() override;

private: // QStyledItemDelegate
	QAbstractItemDelegate * GetDelegate() noexcept override;
	void OnModelChanged() override;
	void SetEnabled(bool enabled) noexcept override;

	void RegisterObserver(IObserver * observer) override;
	void UnregisterObserver(IObserver * observer) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
