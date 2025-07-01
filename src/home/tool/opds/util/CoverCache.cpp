#include "CoverCache.h"

#include <mutex>
#include <unordered_map>

#include <QString>
#include <QTimer>

#include "util/FunctorExecutionForwarder.h"

using namespace HomeCompa::Opds;

struct CoverCache::Impl
{
	QTimer coversTimer;
	std::mutex coversGuard;
	std::unordered_map<QString, QByteArray> covers;
	Util::FunctorExecutionForwarder forwarder;

	Impl()
	{
		coversTimer.setInterval(std::chrono::minutes(1));
		coversTimer.setSingleShot(true);
		QObject::connect(&coversTimer,
		                 &QTimer::timeout,
		                 [this]
		                 {
							 std::lock_guard lock(coversGuard);
							 covers.clear();
						 });
	}
};

CoverCache::CoverCache()
	: m_impl { std::make_unique<Impl>() }
{
}

CoverCache::~CoverCache() = default;

void CoverCache::Set(QString id, QByteArray data) const
{
	auto& impl = *m_impl;
	std::lock_guard lock(impl.coversGuard);
	impl.covers.try_emplace(std::move(id), std::move(data));
	impl.forwarder.Forward([&] { impl.coversTimer.start(); });
}

QByteArray CoverCache::Get(const QString& id) const
{
	auto& impl = *m_impl;
	std::lock_guard lock(impl.coversGuard);
	auto it = impl.covers.find(id);
	if (it == impl.covers.end())
		return {};

	auto result = std::move(it->second);
	impl.covers.erase(it);
	return result;
}
