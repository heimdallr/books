#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "util/ISettings.h"

namespace HomeCompa::Flibrary {

class GeometryRestorable
{
	NON_COPY_MOVABLE(GeometryRestorable)

protected:
	class IObserver  // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~IObserver() = default;
		virtual QWidget & GetWidget() noexcept = 0;
		virtual void OnFontSizeChanged(int /*fontSize*/) {}
	};

protected:
	GeometryRestorable(IObserver & observer, std::shared_ptr<ISettings> settings, QString name);
	~GeometryRestorable();

protected:
	virtual void Init();

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
