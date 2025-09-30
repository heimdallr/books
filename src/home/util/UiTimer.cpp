#include "UiTimer.h"

#include <QTimer>

namespace HomeCompa::Util
{

std::unique_ptr<QTimer> CreateUiTimer(std::function<void()> f)
{
	auto timer = std::make_unique<QTimer>();
	timer->setSingleShot(true);
	timer->setInterval(std::chrono::milliseconds(200));
	QObject::connect(timer.get(), &QTimer::timeout, timer.get(), [f = std::move(f)] {
		f();
	});

	return timer;
}

}
