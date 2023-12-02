#include "ZipProgressCallback.h"

#include <plog/Log.h>

#include "fnd/observable.h"

using namespace HomeCompa::Flibrary;

struct ZipProgressCallback::Impl : Observable<IObserver>
{
	std::atomic_bool stopped { false };
	int64_t total { 0 };
	int64_t progress { 0 };
	int percents { 0 };

	void Stop()
	{
		stopped = true;
	}
};

ZipProgressCallback::ZipProgressCallback()
{
	PLOGD << "ZipProgressCallback created";
}
ZipProgressCallback::~ZipProgressCallback()
{
	m_impl->Perform(&IObserver::OnProgress, 100);
	PLOGD << "ZipProgressCallback destroyed";
}

void ZipProgressCallback::Stop()
{
	m_impl->Stop();
	PLOGD << "ZipProgressCallback stopped";
}

void ZipProgressCallback::RegisterObserver(IObserver * observer)
{
	m_impl->Register(observer);
}

void ZipProgressCallback::UnregisterObserver(IObserver * observer)
{
	m_impl->Unregister(observer);
}

void ZipProgressCallback::OnStartWithTotal(const int64_t totalBytes)
{
	m_impl->total = totalBytes;
	m_impl->Perform(&IObserver::OnProgress, 0);
}

void ZipProgressCallback::OnIncrement(const int64_t bytes)
{
	m_impl->progress += bytes;
	const auto percents = static_cast<int>(100 * m_impl->progress / m_impl->total);
	if (m_impl->percents == percents)
		return;

	m_impl->percents = percents;
	m_impl->Perform(&IObserver::OnProgress, percents);
}

void ZipProgressCallback::OnDone()
{
	m_impl->Perform(&IObserver::OnProgress, 100);
	m_impl->Stop();
}

void ZipProgressCallback::OnFileDone(const QString & /*filePath*/)
{
}

bool ZipProgressCallback::OnCheckBreak()
{
	return m_impl->stopped;
}
