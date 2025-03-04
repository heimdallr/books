#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IAnnotationController.h"
#include "interface/logic/IDataItem.h"

#include "data/DataItem.h"

namespace HomeCompa::Flibrary
{

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
		QString language, sourceLanguage;
		std::vector<QString> keywords;
		IAnnotationController::IDataProvider::Covers covers;
		std::optional<size_t> coverIndex;
		IDataItem::Ptr content { NavigationItem::Create() };
		IDataItem::Ptr translators { NavigationItem::Create() };
		PublishInfo publishInfo;
		QString error;
		size_t textSize { 0 };
	};

public:
	ArchiveParser(std::shared_ptr<class ICollectionProvider> collectionProvider,
	              std::shared_ptr<class IAnnotationProgressController> progressController,
	              std::shared_ptr<const class ILogicFactory> logicFactory);
	~ArchiveParser();

public:
	[[nodiscard]] std::shared_ptr<class IProgressController> GetProgressController() const;
	[[nodiscard]] Data Parse(const IDataItem& dataItem) const;
	void Stop() const;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
