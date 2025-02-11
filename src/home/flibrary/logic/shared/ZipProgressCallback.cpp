#include "ZipProgressCallback.h"

#include <plog/Log.h>

#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IProgressController.h"

using namespace HomeCompa::Flibrary;

struct ZipProgressCallback::Impl
{
	std::shared_ptr<IProgressController> progressController;
	std::unique_ptr<IProgressController::IProgressItem> progressItem;

	std::atomic_bool stopped { false };
	int64_t total { 0 };
	int64_t progress { 0 };
	int percents { 0 };

	void Stop()
	{
		progressController->Stop();
		stopped = true;
	}

	explicit Impl(std::shared_ptr<IProgressController> progressController)
		: progressController { std::move(progressController) }
		, progressItem { this->progressController->Add(100) }

	{
	}
};

ZipProgressCallback::ZipProgressCallback(const std::shared_ptr<const ILogicFactory> & logicFactory)
	: m_impl(logicFactory->GetProgressController())
{
	PLOGV << "ZipProgressCallback created";
}
ZipProgressCallback::~ZipProgressCallback()
{
	PLOGV << "ZipProgressCallback destroyed";
}

void ZipProgressCallback::Stop()
{
	m_impl->Stop();
	PLOGD << "ZipProgressCallback stopped";
}

void ZipProgressCallback::OnStartWithTotal(const int64_t totalBytes)
{
	m_impl->total = totalBytes;
}

void ZipProgressCallback::OnIncrement(const int64_t bytes)
{
	OnSetCompleted(m_impl->progress + bytes);
}

void ZipProgressCallback::OnSetCompleted(const int64_t bytes)
{
	if (!m_impl->total)
		return;

	m_impl->progress = std::min(bytes, m_impl->total);
	const auto percents = static_cast<int>(100 * m_impl->progress / m_impl->total);
	if (m_impl->percents == percents)
		return;

	assert(m_impl->progressItem);
	m_impl->progressItem->Increment(percents - m_impl->percents);

	m_impl->percents = percents;
}

void ZipProgressCallback::OnDone()
{
	m_impl->progressItem->Increment(100 - m_impl->percents);
	m_impl->percents = 100;
}

void ZipProgressCallback::OnFileDone(const QString & /*filePath*/)
{
}

bool ZipProgressCallback::OnCheckBreak()
{
	return m_impl->progressItem->IsStopped();
}
