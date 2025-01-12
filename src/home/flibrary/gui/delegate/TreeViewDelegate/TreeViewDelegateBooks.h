#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/ui/ITreeViewDelegate.h"

class QAbstractScrollArea;
class QString;
class QVariant;

namespace HomeCompa::Flibrary {

class TreeViewDelegateBooks final : public ITreeViewDelegate
{
	NON_COPY_MOVABLE(TreeViewDelegateBooks)

public:
	using TextDelegate = QString(*)(const QVariant & value);

public:
	explicit TreeViewDelegateBooks(const std::shared_ptr<const class IUiFactory> & uiFactory);
	~TreeViewDelegateBooks() override;

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
