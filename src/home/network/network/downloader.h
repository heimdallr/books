#pragma once

#include <functional>

#include <QHttpHeaders>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "export/network.h"

class QHttpHeaders;
class QIODevice;
class QString;

namespace HomeCompa::Network
{

class NETWORK_EXPORT Downloader
{
	NON_COPY_MOVABLE(Downloader)

public:
	using OnFinish   = std::function<void(size_t id, int code, const QString& message)>;
	using OnProgress = std::function<void(int64_t bytesReceived, int64_t bytesTotal, bool& stopped)>;

public:
	Downloader();
	~Downloader();

public:
	size_t Download(const QString& url, QIODevice& io, OnFinish callback, OnProgress progress = {}, const QHttpHeaders& headers = {});

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
