#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "zip.h"

namespace HomeCompa::Flibrary {

class IProgressController;

class ZipProgressCallback final : public Zip::ProgressCallback
{
	NON_COPY_MOVABLE(ZipProgressCallback)

public:
	explicit ZipProgressCallback(std::shared_ptr<IProgressController> progressController);
	~ZipProgressCallback() override;

public:
	void Stop();

private: // ProgressCallback
	void OnStartWithTotal(int64_t totalBytes) override;
	void OnIncrement(int64_t bytes) override;
	void OnDone() override;
	void OnFileDone(const QString & filePath) override;
	bool OnCheckBreak() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
