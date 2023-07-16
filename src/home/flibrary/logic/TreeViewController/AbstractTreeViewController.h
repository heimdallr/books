#pragma once

#include <algorithm>
#include <iterator>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/ITreeViewController.h"

class QString;
class QVariant;

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class AbstractTreeViewController : virtual public ITreeViewController
{
	NON_COPY_MOVABLE(AbstractTreeViewController)

protected:
	explicit AbstractTreeViewController(const char * context
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<class DataProvider> dataProvider
	);
	~AbstractTreeViewController() override;

private: // ITreeViewController
	[[nodiscard]] const char * TrContext() const noexcept override;
	[[nodiscard]] int GetModeIndex() const override;
	void RegisterObserver(IObserver * observer) override;
	void UnregisterObserver(IObserver * observer) override;

protected:
	void Setup();
	void SetMode(int index);
	void SetMode(const QString & mode);
	void Stub(const QVariant &){}

	template<typename Value, size_t ArraySize>
	std::vector<const char *> GetModeNamesImpl(Value(&array)[ArraySize]) const
	{
		std::vector<const char *> result;
		result.reserve(ArraySize);
		std::transform(std::cbegin(array), std::cend(array), std::back_inserter(result), [] (const auto & item) { return item.first; });
		return result;
	}

protected:
	virtual void OnModeChanged(const QVariant & mode) = 0;
	[[nodiscard]] virtual int GetModeIndex(const QVariant & mode) const = 0;

protected:
	const char * const m_context;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	PropagateConstPtr<DataProvider, std::shared_ptr> m_dataProvider;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
