#pragma once
#include <cstdint>

class QString;

namespace HomeCompa::Zip::Impl::SevenZip {

class ProgressCallback
{
public:
	virtual ~ProgressCallback() = default;
	/*
	Called at beginning
	*/
	virtual void OnStartWithTotal(const QString & archivePath, uint64_t totalBytes) = 0;

	/*
	Called Whenever progress has updated with a bytes complete
	*/
	virtual void OnProgress(const QString & archivePath, uint64_t bytesCompleted) = 0;

	/*
	Called When progress has reached 100%
	*/
	virtual void OnDone(const QString & archivePath) = 0;

	/*
	Called When single file progress has reached 100%, returns the filepath that completed
	*/
	virtual void OnFileDone(const QString & archivePath, const QString & filePath, uint64_t bytesCompleted) = 0;

	/*
	Called to determine if it's time to abort the zip operation. Return true to abort the current operation.
	*/
	virtual bool OnCheckBreak() = 0;
};

class ProgressCallbackStub final
	: public ProgressCallback
{
public:
	void OnStartWithTotal(const QString &, uint64_t) override {}
	void OnProgress(const QString &, uint64_t) override { }
	void OnDone(const QString &) override { }
	void OnFileDone(const QString &, const QString &, uint64_t) override { }
	bool OnCheckBreak() override { return false; }
};

}
