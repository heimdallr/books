#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

namespace std::filesystem {
class path;
}

namespace HomeCompa::Flibrary {

class ReaderController
{
	NON_COPY_MOVABLE(ReaderController)

public:
	ReaderController();
	~ReaderController();

public:
	void StartReader(const std::filesystem::path & archive, const std::string & file);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}