#pragma once

#include <QDialog>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

namespace Ui { class CollectionCleaner; };

namespace HomeCompa {
	class ISettings;
}

namespace HomeCompa::Flibrary {

class CollectionCleaner final : public QDialog
{
public:
	CollectionCleaner(std::shared_ptr<ISettings> settings
		, std::shared_ptr<class ILanguageModel> languageModel
		, QWidget *parent = nullptr
	);
	~CollectionCleaner();

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}