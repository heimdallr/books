#pragma once

#include <vector>
#include <QString>

#include "data/DataItem.h"
#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/IDataItem.h"

namespace HomeCompa::Flibrary {

class ArchiveParser final
{
	NON_COPY_MOVABLE(ArchiveParser)

public:
	struct Data
	{
		QString annotation;
		QString epigraph;
		QString epigraphAuthor;
		std::vector<QString> keywords;
		std::vector<QByteArray> covers;
		int coverIndex { -1 };
		IDataItem::Ptr content { NavigationItem::Create() };
	};

public:
	explicit ArchiveParser(std::shared_ptr<class ICollectionController> collectionController);
	~ArchiveParser();

public:
	[[nodiscard]] Data Parse(const IDataItem & dataItem) const;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
