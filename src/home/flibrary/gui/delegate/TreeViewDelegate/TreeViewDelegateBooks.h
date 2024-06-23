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
	TreeViewDelegateBooks(const std::shared_ptr<const class IUiFactory> & uiFactory
		, std::shared_ptr<const class IRateStarsProvider> rateStarsProvider
	);
	~TreeViewDelegateBooks() override;

private: // QStyledItemDelegate
	QAbstractItemDelegate * GetDelegate() override;
	void OnModelChanged() override;

	void RegisterObserver(IObserver * observer) override;
	void UnregisterObserver(IObserver * observer) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
