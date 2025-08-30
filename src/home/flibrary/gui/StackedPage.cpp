#include "StackedPage.h"

#include <QTimer>

#include "interface/constants/ObjectConnectionID.h"

#include "util/ObjectsConnector.h"

using namespace HomeCompa;
using namespace Flibrary;

StackedPage::StackedPage(QWidget* parent)
	: QWidget(parent)
{
	Util::ObjectsConnector::registerEmitter(ObjectConnectorID::STACKED_PAGE_STATE_CHANGED, this, SIGNAL(StateChanged(std::shared_ptr<QWidget>, int)), true);
	QTimer::singleShot(0, [this] { emit StateChanged(shared_from_this(), State::Created); });
}

StackedPage::~StackedPage() = default;

void StackedPage::StateChanged(const int state)
{
	emit StateChanged(shared_from_this(), state);
}
