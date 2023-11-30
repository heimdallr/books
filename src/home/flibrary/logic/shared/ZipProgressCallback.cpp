#include "ZipProgressCallback.h"

#include "fnd/NonCopyMovable.h"

#include "interface/logic/IProgressController.h"

using namespace HomeCompa::Flibrary;

struct ZipProgressCallback::Impl : private IProgressController::IObserver
{
	PropagateConstPtr<IProgressController, std::shared_ptr> progressController;
	PropagateConstPtr<IProgressController::IProgressItem> progressItem;
	std::atomic_bool stopped { false };

	explicit Impl(std::shared_ptr<IProgressController> progressController)
		: progressController(std::move(progressController))
		, progressItem(std::unique_ptr<IProgressController::IProgressItem>{})
	{
		this->progressController->RegisterObserver(this);
	}

	~Impl() override
	{
		progressController->UnregisterObserver(this);
	}

	void Stop()
	{
		stopped = true;
		progressItem.reset();
	}

private: // IProgressController::IObserver
	void OnStartedChanged() override
	{
	}
	void OnValueChanged() override
	{
	}
	void OnStop() override
	{
		Stop();
	}
	NON_COPY_MOVABLE(Impl)
};

ZipProgressCallback::ZipProgressCallback(std::shared_ptr<IProgressController> progressController)
	: m_impl(std::move(progressController))
{
}

ZipProgressCallback::~ZipProgressCallback() = default;

void ZipProgressCallback::Stop()
{
	m_impl->Stop();
}

void ZipProgressCallback::OnStartWithTotal(const int64_t totalBytes)
{
	m_impl->stopped = false;
	if (totalBytes > 2ll * 1024 * 1024)
		m_impl->progressItem.reset(m_impl->progressController->Add(totalBytes));
}

void ZipProgressCallback::OnIncrement(const int64_t bytes)
{
	if (m_impl->progressItem)
		m_impl->progressItem->Increment(bytes);
}

void ZipProgressCallback::OnDone()
{
	m_impl->Stop();
}

void ZipProgressCallback::OnFileDone(const QString & /*filePath*/)
{
}

bool ZipProgressCallback::OnCheckBreak()
{
	return m_impl->stopped;
}
