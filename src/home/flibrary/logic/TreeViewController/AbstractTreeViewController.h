#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"
#include "fnd/observable.h"

#include "interface/logic/IModelProvider.h"
#include "interface/logic/ITreeViewController.h"

#include "util/ISettings.h"

class QString;

namespace HomeCompa::Flibrary
{

class AbstractTreeViewController
	: virtual public ITreeViewController
	, public Observable<ITreeViewController::IObserver>
{
	NON_COPY_MOVABLE(AbstractTreeViewController)

protected:
	explicit AbstractTreeViewController(const char* context, std::shared_ptr<ISettings> settings, const std::shared_ptr<const IModelProvider>& modelProvider);
	~AbstractTreeViewController() override;

private: // ITreeViewController
	[[nodiscard]] const char* TrContext() const noexcept override;
	[[nodiscard]] int GetModeIndex() const override;
	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;
	void SetMode(const QString& mode) override;

	void OnDoubleClicked(const QModelIndex&) const override
	{
	}

	CreateNewItem GetNewItemCreator() const override
	{
		return {};
	}

	RemoveItems GetRemoveItems() const override
	{
		return {};
	}

protected:
	void Setup();

protected:
	virtual void OnModeChanged(const QString& mode) = 0;
	[[nodiscard]] virtual int GetModeIndex(const QString& mode) const = 0;

protected:
	const char* const m_context;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	std::weak_ptr<const IModelProvider> m_modelProvider;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
