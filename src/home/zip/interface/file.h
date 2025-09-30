#pragma once

class Stream;

namespace HomeCompa::ZipDetails
{

class IFile
{
public:
	virtual ~IFile()                        = default;
	virtual std::unique_ptr<Stream> Read()  = 0;
	virtual std::unique_ptr<Stream> Write() = 0;
};

}
