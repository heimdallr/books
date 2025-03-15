#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/ILogicFactory.h"

#include "zip.h"

namespace HomeCompa::Flibrary
{

class ZipProgressCallback final : public Zip::ProgressCallback
{
	NON_COPY_MOVABLE(ZipProgressCallback)

public:
	explicit ZipProgressCallback(const std::shared_ptr<const ILogicFactory>& logicFactory);
	~ZipProgressCallback() override;

public:
	void Stop();

private: // ProgressCallback
	void OnStartWithTotal(int64_t totalBytes) override;
	void OnIncrement(int64_t bytes) override;
	void OnSetCompleted(int64_t bytes) override;
	void OnDone() override;
	void OnFileDone(const QString& filePath) override;
	bool OnCheckBreak() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
