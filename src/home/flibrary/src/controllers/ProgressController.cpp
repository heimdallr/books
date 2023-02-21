#include <mutex>
#include <vector>

#include <QTimer>

#include <plog/Log.h>

#include "util/FunctorExecutionForwarder.h"

#include "ProgressController.h"

namespace HomeCompa::Flibrary {

struct ProgressController::Impl
{
	Util::FunctorExecutionForwarder forwarder;
	QTimer timer;
	std::vector<std::shared_ptr<IProgress>> progress;
	std::mutex progressGuard;
	double value { 0.0 };
};

ProgressController::ProgressController(QObject * parent)
	: QObject(parent)
{
	m_impl->timer.setSingleShot(false);
	m_impl->timer.setInterval(std::chrono::milliseconds(200));
	connect(&m_impl->timer, &QTimer::timeout, this, [&] ()
	{
		std::lock_guard lock(m_impl->progressGuard);

		if (m_impl->progress.empty())
		{
			m_impl->timer.stop();
			m_impl->value = 0.0;
			emit StartedChanged();
			return;
		}

		const auto progress = std::accumulate(std::cbegin(m_impl->progress), std::cend(m_impl->progress), std::pair<size_t, size_t>{}, [] (const auto & init, const auto & progress)
		{
			return std::make_pair(init.first + progress->GetProgress(), init.second + progress->GetTotal());
		});

		if (progress.first < progress.second)
		{
			m_impl->value = static_cast<double>(progress.first) / static_cast<double>(progress.second);
		}
		else
		{
			m_impl->progress.clear();
			m_impl->value = 1.0;
		}

		emit ProgressChanged();
	});

	PLOGD << "ProgressController created";
}

ProgressController::~ProgressController()
{
	PLOGD << "ProgressController destroyed";
}

void ProgressController::Add(std::shared_ptr<IProgress> progress)
{
	std::lock_guard lock(m_impl->progressGuard);
	m_impl->progress.push_back(std::move(progress));
	m_impl->forwarder.Forward([&]
	{
		if (m_impl->progress.size() == 1)
		{
			m_impl->value = 0.0;
			emit ProgressChanged();
		}
		m_impl->timer.start();
		emit StartedChanged();
	});
}

bool ProgressController::GetStarted()
{
	return m_impl->timer.isActive();
}

double ProgressController::GetProgress()
{
	return m_impl->value;
}

void ProgressController::SetStarted([[maybe_unused]] const bool value)
{
	assert(!value);
	std::lock_guard lock(m_impl->progressGuard);
	for (auto & item : m_impl->progress)
		item->Stop();
}

}
