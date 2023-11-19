#pragma once

class QIODevice;

namespace HomeCompa::Zip {

class IFile
{
public:
	virtual ~IFile() = default;
	virtual QIODevice& Read() = 0;
};

}
