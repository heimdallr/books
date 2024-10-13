#include "GeometryRestorable.h"

#include <QEvent>
#include <QSplitter>
#include <QTimer>

#include "fnd/FindPair.h"

#include "util/ISettingsObserver.h"
#include "util/UiTimer.h"
#include "util/serializer/Font.h"

using namespace HomeCompa::Util;

namespace {
constexpr auto GEOMETRY_KEY_TEMPLATE = "ui/%1/Geometry";
constexpr auto SPLITTER_KEY_TEMPLATE = "ui/%1/%2";
constexpr auto FONT_KEY = "ui/Font";
}

class GeometryRestorable::Impl final
	: QObject
	, ISettingsObserver
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

		if (!m_initialized)
			return;

		m_settings->Set(QString(GEOMETRY_KEY_TEMPLATE).arg(m_name), m_observer.GetWidget().geometry());
		for (const auto * splitter : m_observer.GetWidget().findChildren<QSplitter *>())
			m_settings->Set(QString(SPLITTER_KEY_TEMPLATE).arg(m_name).arg(splitter->objectName()), splitter->saveState());
	}

	void Init()
	{
		m_observer.GetWidget().installEventFilter(this);
		for (auto* splitter : m_observer.GetWidget().findChildren<QSplitter*>())
			if (const auto value = m_settings->Get(QString(SPLITTER_KEY_TEMPLATE).arg(m_name).arg(splitter->objectName())); value.isValid())
				splitter->restoreState(value.toByteArray());
	}

	void OnShow()
	{
		OnFontChanged();
		if (const auto value = m_settings->Get(QString(GEOMETRY_KEY_TEMPLATE).arg(m_name)); value.isValid())
			m_observer.GetWidget().setGeometry(value.toRect());

		m_initialized = true;
	}

private: // QObject
	bool eventFilter(QObject * target, QEvent * event) override
	{
		if (event->type() == QEvent::Show)
			OnShow();

		return m_observer.GetWidget().eventFilter(target, event);
	}

private: // ISettingsObserver
	void HandleValueChanged(const QString & key, const QVariant &) override
	{
		if (key.startsWith(FONT_KEY))
			m_fontTimer->start();
	}

private:
	void OnFontChanged()
	{
		const SettingsGroup group(*m_settings, FONT_KEY);
		auto font = m_observer.GetWidget().font();
		Deserialize(font, *m_settings);

		EnumerateWidgets(m_observer.GetWidget(), [&] (QWidget & widget)
		{
			widget.setFont(font);
		});

		m_observer.OnFontChanged(font);
	}

	static void EnumerateWidgets(QWidget & parent, const std::function<void(QWidget &)> & f)
	{
		f(parent);
		for (auto * widget : parent.findChildren<QWidget *>())
			f(*widget);
	}

private:
	IObserver & m_observer;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	const QString m_name;
	bool m_initialized { false };
	PropagateConstPtr<QTimer> m_fontTimer { CreateUiTimer([&] { OnFontChanged(); }) };
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

GeometryRestorableObserver::GeometryRestorableObserver(QWidget & widget)
	: m_widget(widget)
{
}

QWidget & GeometryRestorableObserver::GetWidget() noexcept
{
	return m_widget;
}
