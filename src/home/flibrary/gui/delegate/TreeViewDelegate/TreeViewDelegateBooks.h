#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/ui/ITreeViewDelegate.h"

namespace HomeCompa
{
class ISettings;
}

class QAbstractScrollArea;
class QString;
class QVariant;

namespace HomeCompa::Flibrary
{

class TreeViewDelegateBooks final : public ITreeViewDelegate
{
	NON_COPY_MOVABLE(TreeViewDelegateBooks)

public:
	using TextDelegate = QString (*)(const QVariant& value);

public:
	TreeViewDelegateBooks(const std::shared_ptr<const class IUiFactory>& uiFactory, const std::shared_ptr<const ISettings>& settings);
	~TreeViewDelegateBooks() override;

private: // QStyledItemDelegate
	QAbstractItemDelegate* GetDelegate() noexcept override;
	void                   OnModelChanged() override;
	void                   SetEnabled(bool enabled) noexcept override;

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
