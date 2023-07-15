#include <QWidget>

#include "fnd/FindPair.h"
#include "util/ISettingsObserver.h"
#include "interface/constants/SettingsConstant.h"

#include "GeometryRestorable.h"

#include <qcoreevent.h>

using namespace HomeCompa::Flibrary;

namespace {
constexpr auto GEOMETRY_KEY_TEMPLATE = "ui/%1/Geometry";

class ISettingsObserverHandler  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ISettingsObserverHandler() = default;
	virtual void OnFontSizeChanged(const QVariant & value) = 0;
	virtual void Stub(const QVariant & /*value*/) {}
};

using Handler = void(ISettingsObserverHandler::*)(const QVariant &);
constexpr std::pair<const char *, Handler> HANDLERS[]
{
	{Constant::Settings::FONT_SIZE_KEY, &ISettingsObserverHandler::OnFontSizeChanged},
};

}

class GeometryRestorable::Impl final
	: QObject
	, ISettingsObserver
	, ISettingsObserverHandler
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(IObserver & observer, std::shared_ptr<ISettings> settings, QString name)
		: m_observer(observer)
		, m_settings(std::move(settings))
		, m_name(std::move(name))
	{
		m_settings->RegisterObserver(this);
	}

	~Impl() override
	{
		m_settings->UnregisterObserver(this);
		m_settings->Set(QString(GEOMETRY_KEY_TEMPLATE).arg(m_name), m_observer.GetWidget().geometry());
	}

	void Init()
	{
		m_observer.GetWidget().installEventFilter(this);
	}

	void OnShow()
	{
		OnFontSizeChanged(m_settings->Get(Constant::Settings::FONT_SIZE_KEY, Constant::Settings::FONT_SIZE_DEFAULT));
		if (const auto value = m_settings->Get(QString(GEOMETRY_KEY_TEMPLATE).arg(m_name)); value.isValid())
			m_observer.GetWidget().setGeometry(value.toRect());
	}

private: // QObject
	bool eventFilter(QObject * target, QEvent * event) override
	{
		if (event->type() == QEvent::Show)
			OnShow();

		return m_observer.GetWidget().eventFilter(target, event);
	}

private: // ISettingsObserver
	void HandleValueChanged(const QString & key, const QVariant & value) override
	{
		const auto f = FindSecond(HANDLERS, key.toStdString().data(), &ISettingsObserverHandler::Stub, PszComparer {});
		((*this).*f)(value);
	}

private: // ISettingsObserverHandler
	void OnFontSizeChanged(const QVariant & value) override
	{
		bool ok = false;
		const auto size = value.toInt(&ok);
		if (!ok)
			return;

		EnumerateWidgets(m_observer.GetWidget(), [&] (QWidget & widget)
		{
			auto font = widget.font();
			font.setPointSize(size);
			widget.setFont(font);
		});

		m_observer.OnFontSizeChanged(size);
	}

private:
	static void EnumerateWidgets(const QWidget & parent, const std::function<void(QWidget &)> & f)
	{
		for (auto * widget : parent.findChildren<QWidget *>())
			f(*widget);
	}

private:
	IObserver & m_observer;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	const QString m_name;
};

GeometryRestorable::GeometryRestorable(IObserver & observer, std::shared_ptr<ISettings> settings, QString name)
	: m_impl(observer, std::move(settings), std::move(name))
{
}

GeometryRestorable::~GeometryRestorable() = default;

void GeometryRestorable::Init()
{
	m_impl->Init();
}
