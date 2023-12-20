#pragma once

#include <functional>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "export/network.h"

class QIODevice;
class QString;

namespace HomeCompa::Network {

class NETWORK_EXPORT Downloader
{
	NON_COPY_MOVABLE(Downloader)

public:
	using OnFinish = std::function<void(int code, const QString & message)>;
	using OnProgress = std::function<void(int64_t bytesReceived, int64_t bytesTotal, bool & stopped)>;

public:
	Downloader();
	~Downloader();

public:
	void Download(const QString & url, QIODevice & io, OnFinish callback, OnProgress progress = {});

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
