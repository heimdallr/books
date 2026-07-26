#include "StackedPage.h"

#include <QAction>
#include <QTimer>

#include "interface/constants/ObjectConnectionID.h"
#include "interface/ui/IUiFactory.h"

#include "util/ObjectsConnector.h"

using namespace HomeCompa;
using namespace Flibrary;

StackedPage::StackedPage(const IUiFactory& uiFactory, QWidget* parent)
	: QWidget(parent)
	, closeAction { new QAction(this) }
	, stackedWidget { uiFactory.GetStackedWidget() }
{
	Util::ObjectsConnector::registerEmitter(ObjectConnectorID::STACKED_PAGE_STATE_CHANGED, this, SIGNAL(StateChanged(QStackedWidget*, std::shared_ptr<QWidget>, int)), true);
	QTimer::singleShot(0, [this] {
		StateChanged(State::Created);
	});

	closeAction->setShortcut(QKeySequence(static_cast<int>(Qt::SHIFT) | static_cast<int>(Qt::Key_Escape)));
	addAction(closeAction);
	connect(closeAction, &QAction::triggered, [this] {
		StateChanged(State::Finished);
	});
}

StackedPage::~StackedPage() = default;

void StackedPage::StateChanged(const int state)
{
	emit StateChanged(stackedWidget, shared_from_this(), state);
}
