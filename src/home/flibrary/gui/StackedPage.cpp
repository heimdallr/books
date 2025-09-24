#include "StackedPage.h"

#include <QTimer>

#include "interface/constants/ObjectConnectionID.h"

#include "util/ObjectsConnector.h"

using namespace HomeCompa;
using namespace Flibrary;

StackedPage::StackedPage(QWidget* parent)
	: QWidget(parent)
	, closeAction { new QAction(this) }
{
	Util::ObjectsConnector::registerEmitter(ObjectConnectorID::STACKED_PAGE_STATE_CHANGED, this, SIGNAL(StateChanged(std::shared_ptr<QWidget>, int)), true);
	QTimer::singleShot(0, [this] { StateChanged(State::Created); });

	closeAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Escape));
	addAction(closeAction);
	connect(closeAction, &QAction::triggered, [this] { StateChanged(State::Finished); });
}

StackedPage::~StackedPage() = default;

void StackedPage::StateChanged(const int state)
{
	emit StateChanged(shared_from_this(), state);
}
