#pragma once

namespace HomeCompa::Flibrary
{

class IInpxGenerator // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Callback = std::function<void(bool)>;

public:
	virtual ~IInpxGenerator() = default;

	virtual void ExtractAsInpxCollection(QString folder, const std::vector<QString>& idList, const class IBookInfoProvider& dataProvider, Callback callback) = 0;
	virtual void GenerateInpx(QString inpxFileName, const std::vector<QString>& idList, const IBookInfoProvider& dataProvider, Callback callback) = 0;
};

} // namespace HomeCompa::Flibrary
