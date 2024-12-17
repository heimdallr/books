#include "ItemViewToolTipper.h"

#include <QAbstractItemView>
#include <QHelpEvent>
#include <QTimer>
#include <QToolTip>

#include <plog/Log.h>

#include "interface/constants/SettingsConstant.h"
#include "util/ISettings.h"
#include "util/ISettingsObserver.h"
#include "util/UiTimer.h"

using namespace HomeCompa::Flibrary;

struct ItemViewToolTipper::Impl final
	: ISettingsObserver
{
	PropagateConstPtr<ISettings, std::shared_ptr> settings;
	int fontSize { settings->Get(Constant::Settings::FONT_SIZE_KEY, Constant::Settings::FONT_SIZE_DEFAULT) };
	QString fontFamily { settings->Get(Constant::Settings::FONT_SIZE_FAMILY, QString("Sans Serif"))};

	explicit Impl(std::shared_ptr<ISettings> settings)
		: settings(std::move(settings))
	{
		this->settings->RegisterObserver(this);
	}

	~Impl() override
	{
		settings->UnregisterObserver(this);
	}

private: // ISettingsObserver
	void HandleValueChanged(const QString & key, const QVariant &) override
	{
		if (key.startsWith(Constant::Settings::FONT_KEY))
			m_fontTimer->start();
	}

private:
	void OnFontChanged()
	{
		fontSize = settings->Get(Constant::Settings::FONT_SIZE_KEY, fontSize);
		fontFamily = settings->Get(Constant::Settings::FONT_SIZE_FAMILY, fontFamily);
	}

private:
	PropagateConstPtr<QTimer> m_fontTimer { Util::CreateUiTimer([&] { OnFontChanged(); }) };
	NON_COPY_MOVABLE(Impl)
};

ItemViewToolTipper::ItemViewToolTipper(std::shared_ptr<ISettings> settings
	, QObject * parent
)
	: QObject(parent)
	, m_impl(std::move(settings))
{
	PLOGV << "ItemViewToolTipper created";
}

ItemViewToolTipper::~ItemViewToolTipper()
{
	PLOGV << "ItemViewToolTipper destroyed";
}

bool ItemViewToolTipper::eventFilter(QObject * obj, QEvent * event)
{
	if (event->type() != QEvent::ToolTip)
		return false;

	auto * view = qobject_cast<QAbstractItemView *>(obj->parent());
	if (!(view && view->model()))
		return false;

	const auto * helpEvent = static_cast<const QHelpEvent *>(event);  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

	const auto pos = helpEvent->pos();
	const auto index = view->indexAt(pos);
	if (!index.isValid())
		return false;

	const auto & model = *view->model();
	const auto itemTooltip = model.data(index, Qt::ToolTipRole).toString();
	if (itemTooltip.isEmpty())
		return false;

	const auto itemText = model.data(index).toString();
	const int itemTextWidth = QFontMetrics(view->font()).horizontalAdvance(itemText);

	const auto rect = view->visualRect(index);
	auto rectWidth = rect.width();

	if (model.flags(index) & Qt::ItemIsUserCheckable)
		rectWidth -= rect.height();

	static constexpr auto richTextTemplate = R"(<p style=" font-family:%1; font-size:%2pt; ">%3</p>)";

	itemTextWidth > rectWidth
		? QToolTip::showText(helpEvent->globalPos(), QString(richTextTemplate).arg(m_impl->fontFamily).arg(m_impl->fontSize * 11 / 10).arg(itemTooltip), view)
		: QToolTip::hideText();

	return true;
}
