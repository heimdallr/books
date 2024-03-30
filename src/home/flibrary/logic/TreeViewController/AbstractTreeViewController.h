#pragma once

#include <algorithm>
#include <iterator>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "fnd/observable.h"

#include "interface/logic/ITreeViewController.h"

class QString;

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class AbstractTreeViewController
	: virtual public ITreeViewController
	, public Observable<ITreeViewController::IObserver>
{
	NON_COPY_MOVABLE(AbstractTreeViewController)

protected:
	explicit AbstractTreeViewController(const char * context
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<class DataProvider> dataProvider
		, const std::shared_ptr<const class IModelProvider>& modelProvider
	);
	~AbstractTreeViewController() override;

private: // ITreeViewController
	[[nodiscard]] const char * TrContext() const noexcept override;
	[[nodiscard]] int GetModeIndex() const override;
	void RegisterObserver(IObserver * observer) override;
	void UnregisterObserver(IObserver * observer) override;
	void SetMode(const QString & mode) override;
	void OnDoubleClicked(const QModelIndex &) const override {}

protected:
	void Setup();

	template<typename Value, size_t ArraySize>
	std::vector<const char *> GetModeNamesImpl(Value(&array)[ArraySize]) const
	{
		std::vector<const char *> result;
		result.reserve(ArraySize);
		std::transform(std::cbegin(array), std::cend(array), std::back_inserter(result), [] (const auto & item) { return item.first; });
		return result;
	}

protected:
	virtual void OnModeChanged(const QString & mode) = 0;
	[[nodiscard]] virtual int GetModeIndex(const QString & mode) const = 0;

protected:
	const char * const m_context;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	PropagateConstPtr<DataProvider, std::shared_ptr> m_dataProvider;
	std::weak_ptr<const IModelProvider> m_modelProvider;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
