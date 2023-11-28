#pragma once

class QIODevice;

namespace HomeCompa::ZipDetails {

class IFile
{
public:
	virtual ~IFile() = default;
	virtual QIODevice & Read() = 0;
	virtual QIODevice & Write() = 0;
};

}
