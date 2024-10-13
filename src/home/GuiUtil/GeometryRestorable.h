#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "util/ISettings.h"

#include "export/GuiUtil.h"

class QSplitter;

namespace HomeCompa::Util {

class GUIUTIL_EXPORT GeometryRestorable
{
	NON_COPY_MOVABLE(GeometryRestorable)

public:
	class IObserver  // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~IObserver() = default;
		virtual QWidget & GetWidget() noexcept = 0;
		virtual void OnFontChanged(const QFont &) {}
	};

protected:
	GeometryRestorable(IObserver & observer, std::shared_ptr<ISettings> settings, QString name);
	~GeometryRestorable();

protected:
	void Init();

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

class GUIUTIL_EXPORT GeometryRestorableObserver
	: virtual public GeometryRestorable::IObserver
{
protected:
	explicit GeometryRestorableObserver(QWidget & widget);

protected: // GeometryRestorable::IObserver
	QWidget & GetWidget() noexcept override;

private:
	QWidget & m_widget;
};

GUIUTIL_EXPORT void InitSplitter(QSplitter * splitter);

}
