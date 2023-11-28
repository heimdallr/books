#pragma once
#include <cstdint>

class QString;

namespace HomeCompa::ZipDetails {

class ProgressCallback
{
public:
	virtual ~ProgressCallback() = default;

	// Called at beginning
	virtual void OnStartWithTotal(uint64_t totalBytes) = 0;

	// Called Whenever progress has updated with a bytes complete
	virtual void OnProgress(uint64_t bytesCompleted) = 0;

	// Called When progress has reached 100%
	virtual void OnDone() = 0;

	// Called When single file progress has reached 100%, returns the filepath that completed
	virtual void OnFileDone(const QString & filePath, uint64_t bytesCompleted) = 0;

	// Called to determine if it's time to abort the zip operation. Return true to abort the current operation.
	virtual bool OnCheckBreak() = 0;
};

}
