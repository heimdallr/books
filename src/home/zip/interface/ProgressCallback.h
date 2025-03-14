#pragma once
#include <cstdint>
#include <functional>

class QIODevice;
class QString;

namespace HomeCompa::ZipDetails
{

using StreamGetter = std::function<std::unique_ptr<QIODevice>(size_t)>;
using SizeGetter = std::function<size_t(size_t)>;

class ProgressCallback // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ProgressCallback() = default;

	virtual void OnStartWithTotal(int64_t totalBytes) = 0;
	virtual void OnSetCompleted(int64_t bytes) = 0;
	virtual void OnIncrement(int64_t bytes) = 0;
	virtual void OnDone() = 0;
	virtual void OnFileDone(const QString& filePath) = 0;
	virtual bool OnCheckBreak() = 0;
};

class ProgressCallbackStub final : public ProgressCallback
{
public:
	void OnStartWithTotal(int64_t) override
	{
	}

	void OnIncrement(int64_t) override
	{
	}

	void OnSetCompleted(int64_t) override
	{
	}

	void OnDone() override
	{
	}

	void OnFileDone(const QString&) override
	{
	}

	bool OnCheckBreak() override
	{
		return false;
	}
};

} // namespace HomeCompa::ZipDetails
