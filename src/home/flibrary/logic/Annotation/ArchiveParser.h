#pragma once

#include <vector>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "data/DataItem.h"

namespace HomeCompa::Flibrary {

class ArchiveParser final
{
	NON_COPY_MOVABLE(ArchiveParser)

public:
	struct Data
	{
		QString annotation;
		std::vector<QString> keywords;
		std::vector<QString> covers;
		int coverIndex { -1 };
		DataItem::Ptr content;
	};

public:
	explicit ArchiveParser(std::shared_ptr<class ICollectionController> collectionController);
	~ArchiveParser();

public:
	[[nodiscard]] Data Parse(const DataItem & dataItem) const;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
