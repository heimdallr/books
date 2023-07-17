#pragma once

#include <vector>

#include "fnd/observer.h"

class QAbstractItemModel;

namespace HomeCompa::Flibrary {

class ITreeViewController  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IObserver : public Observer
	{
	public:
		virtual void OnModeChanged(int index) = 0;
		virtual void OnModelChanged(QAbstractItemModel * model) = 0;
	};

public:
	virtual ~ITreeViewController() = default;
	[[nodiscard]] virtual const char * TrContext() const noexcept = 0;
	[[nodiscard]] virtual std::vector<const char *> GetModeNames() const = 0;
	virtual void SetModeIndex(int index) = 0;
	virtual int GetModeIndex() const = 0;

	virtual void RegisterObserver(IObserver * observer) = 0;
	virtual void UnregisterObserver(IObserver * observer) = 0;
};

}
