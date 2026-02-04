#include "GeometryRestorable.h"

#include <QScreen>
#include <QSplitter>
#include <QTimer>

#include "fnd/FindPair.h"
#include "fnd/algorithm.h"

#include "util/ISettingsObserver.h"
#include "util/UiTimer.h"
#include "util/serializer/Font.h"

using namespace HomeCompa::Util;

namespace
{

constexpr auto STATE_KEY_TEMPLATE    = "ui/%1/State";
constexpr auto GEOMETRY_KEY_TEMPLATE = "ui/%1/Geometry";
constexpr auto SPLITTER_KEY_TEMPLATE = "ui/%1/%2";
constexpr auto FONT_KEY              = "ui/Font";

void InitSplitter(QSplitter* splitter)
{
	QTimer::singleShot(0, [=] {
		QList<int> sizes = splitter->sizes();
		const auto width = std::accumulate(sizes.cbegin(), sizes.cend(), 0);

		QList<int> stretch;
		if (splitter->orientation() == Qt::Horizontal)
			for (int i = 0, sz = splitter->count(); i < sz; ++i)
				stretch << splitter->widget(i)->sizePolicy().horizontalStretch();
		else
			for (int i = 0, sz = splitter->count(); i < sz; ++i)
				stretch << splitter->widget(i)->sizePolicy().verticalStretch();

		const auto weightSum = std::accumulate(stretch.cbegin(), stretch.cend(), 0);
		if (weightSum == 0)
			return;

		std::ranges::transform(stretch, sizes.begin(), [=](const auto weight) {
			return width * weight / weightSum;
		});

		splitter->setSizes(sizes);
	});
}

void SetGeometry(QWidget& widget, QRect rect)
{
	if (std::ranges::none_of(QGuiApplication::screens(), [center = rect.center()](const auto* screen) {
			return screen->availableGeometry().contains(center);
		}))
		rect.moveCenter(QGuiApplication::primaryScreen()->availableGeometry().center());

	widget.setGeometry(rect);
}

} // namespace

class GeometryRestorable::Impl final
	: QObject
	, ISettingsObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(IObserver& observer, std::shared_ptr<ISettings> settings, QString name)
		: m_observer(observer)
		, m_settings(std::move(settings))
		, m_name(std::move(name))
	{
		m_settings->RegisterObserver(this);
	}

	~Impl() override
	{
		m_settings->UnregisterObserver(this);
		assert(!m_initialized);
	}

	void LoadGeometry()
	{
		m_observer.GetWidget().installEventFilter(this);
		auto uniqueCheck = [
#ifndef NDEBUG
							   uniqueList = std::unordered_set<QString> {}
#endif
		](const QString& name) mutable -> const QString& {
#ifndef NDEBUG
			assert(uniqueList.insert(name).second && "names must be unique");
#endif
			return name;
		};

		for (auto* splitter : m_observer.GetWidget().findChildren<QSplitter*>())
		{
			if (const auto value = m_settings->Get(QString(SPLITTER_KEY_TEMPLATE).arg(m_name).arg(uniqueCheck(splitter->objectName()))); value.isValid())
				splitter->restoreState(value.toByteArray());
			else
				InitSplitter(splitter);
			splitter->setChildrenCollapsible(false);
		}
	}

	void OnHide()
	{
		if (!Set(m_initialized, false))
			return;

		const auto state = m_observer.GetWidget().windowState() & (Qt::WindowMaximized | Qt::WindowFullScreen);
		m_settings->Set(QString(STATE_KEY_TEMPLATE).arg(m_name), static_cast<int>(state));

		if ((state & (Qt::WindowMaximized | Qt::WindowFullScreen)) == Qt::WindowNoState)
			m_settings->Set(QString(GEOMETRY_KEY_TEMPLATE).arg(m_name), m_observer.GetWidget().geometry());

		for (const auto* splitter : m_observer.GetWidget().findChildren<QSplitter*>())
			m_settings->Set(QString(SPLITTER_KEY_TEMPLATE).arg(m_name).arg(splitter->objectName()), splitter->saveState());
	}

	void OnShow()
	{
		if (!Set(m_initialized, true))
			return;

		OnFontChanged();

		if (const auto state = m_observer.GetWidget().windowState(); state & (Qt::WindowMaximized | Qt::WindowFullScreen))
			return;

		if (const auto stateVar = m_settings->Get(QString(GEOMETRY_KEY_TEMPLATE).arg(m_name)); stateVar.isValid())
			SetGeometry(m_observer.GetWidget(), stateVar.toRect());

		if (const auto value = m_settings->Get(QString(STATE_KEY_TEMPLATE).arg(m_name)).toInt(); value & (Qt::WindowMaximized | Qt::WindowFullScreen))
			m_observer.GetWidget().setWindowState(static_cast<Qt::WindowState>(value & ~Qt::WindowMinimized));
	}

private: // QObject
	bool eventFilter(QObject* target, QEvent* event) override
	{
		if (event->type() == QEvent::Show)
			OnShow();

		if (event->type() == QEvent::Hide)
			OnHide();

		return m_observer.GetWidget().eventFilter(target, event);
	}

private: // ISettingsObserver
	void HandleValueChanged(const QString& key, const QVariant&) override
	{
		if (key.startsWith(FONT_KEY))
			m_fontTimer->start();
	}

private:
	void OnFontChanged()
	{
		const SettingsGroup group(*m_settings, FONT_KEY);
		auto                font = m_observer.GetWidget().font();
		Deserialize(font, *m_settings);

		EnumerateWidgets(m_observer.GetWidget(), [&](QWidget& widget) {
			widget.setFont(font);
		});

		m_observer.OnFontChanged(font);
	}

	static void EnumerateWidgets(QWidget& parent, const std::function<void(QWidget&)>& f)
	{
		f(parent);
		for (auto* widget : parent.findChildren<QWidget*>())
			f(*widget);
	}

private:
	IObserver&                                    m_observer;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	const QString                                 m_name;
	bool                                          m_initialized { false };
	PropagateConstPtr<QTimer>                     m_fontTimer { CreateUiTimer([&] {
        OnFontChanged();
    }) };
};

GeometryRestorable::GeometryRestorable(IObserver& observer, std::shared_ptr<ISettings> settings, QString name)
	: m_impl(observer, std::move(settings), std::move(name))
{
}

GeometryRestorable::~GeometryRestorable() = default;

void GeometryRestorable::LoadGeometry()
{
	m_impl->LoadGeometry();
}

void GeometryRestorable::SaveGeometry()
{
	m_impl->OnHide();
}

GeometryRestorableObserver::GeometryRestorableObserver(QWidget& widget)
	: m_widget(widget)
{
}

QWidget& GeometryRestorableObserver::GetWidget() noexcept
{
	return m_widget;
}
