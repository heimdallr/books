#pragma once

#include <QMainWindow>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/ui/IMainWindow.h"

namespace HomeCompa
{
class ISettings;
class IParentWidgetProvider;
}

namespace HomeCompa::Flibrary
{

class MainWindow final
	: public QMainWindow
	, virtual public IMainWindow
{
	Q_OBJECT
	NON_COPY_MOVABLE(MainWindow)

public:
	MainWindow(const std::shared_ptr<const class ILogicFactory>& logicFactory,
	           std::shared_ptr<class IUiFactory> uiFactory,
	           std::shared_ptr<ISettings> settings,
	           std::shared_ptr<class ICollectionController> collectionController,
	           std::shared_ptr<class ICollectionUpdateChecker> collectionUpdateChecker,
	           std::shared_ptr<IParentWidgetProvider> parentWidgetProvider,
	           std::shared_ptr<class IAnnotationController> annotationController,
	           std::shared_ptr<class AnnotationWidget> annotationWidget,
	           std::shared_ptr<class LocaleController> localeController,
	           std::shared_ptr<class ILogController> logController,
	           std::shared_ptr<class ProgressBar> progressBar,
	           std::shared_ptr<class LogItemDelegate> logItemDelegate,
	           std::shared_ptr<class ICommandLine> commandLine,
	           std::shared_ptr<class ILineOption> lineOption,
	           std::shared_ptr<class IDatabaseChecker> databaseChecker,
	           QWidget* parent = nullptr);
	~MainWindow() override;

signals:
	void BookTitleToSearchVisibleChanged() const;

private: // IMainWindow
	void Show() override;

private slots:
	void OnBooksSearchFilterValueGeometryChanged(const QRect& geometry);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
