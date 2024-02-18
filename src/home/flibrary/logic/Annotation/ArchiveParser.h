#pragma once

#include "data/DataItem.h"
#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/IAnnotationController.h"
#include "interface/logic/IDataItem.h"

namespace HomeCompa::Flibrary {

class ZipProgressCallback;

class ArchiveParser final
{
	NON_COPY_MOVABLE(ArchiveParser)

public:
	struct Data
	{
		struct PublishInfo
		{
			QString publisher;
			QString city;
			QString year;
			QString isbn;
		};
		QString annotation;
		QString epigraph;
		QString epigraphAuthor;
		std::vector<QString> keywords;
		IAnnotationController::IDataProvider::Covers covers;
		int coverIndex { -1 };
		IDataItem::Ptr content { NavigationItem::Create() };
		IDataItem::Ptr translators { NavigationItem::Create() };
		PublishInfo publishInfo;
		QString error;
	};

public:
	ArchiveParser(std::shared_ptr<class ICollectionController> collectionController
		, std::shared_ptr<class IAnnotationProgressController> progressController
		, std::shared_ptr<ZipProgressCallback> zipProgressCallback
	);
	~ArchiveParser();

public:
	[[nodiscard]] std::shared_ptr<class IProgressController> GetProgressController() const;
	[[nodiscard]] Data Parse(const IDataItem & dataItem) const;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
