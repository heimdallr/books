#pragma once

#include <QMainWindow>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IAnnotationController.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/ICollectionUpdateChecker.h"
#include "interface/logic/ICommandLine.h"
#include "interface/logic/IDatabaseChecker.h"
#include "interface/logic/ILogController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/ui/ILineOption.h"
#include "interface/ui/IMainWindow.h"
#include "interface/ui/IStyleApplierFactory.h"
#include "interface/ui/IUiFactory.h"

#include "GuiUtil/interface/IParentWidgetProvider.h"
#include "util/ISettings.h"

#include "AnnotationWidget.h"
#include "AuthorAnnotationWidget.h"
#include "LocaleController.h"
#include "LogItemDelegate.h"
#include "ProgressBar.h"

namespace HomeCompa::Flibrary
{

class MainWindow final
	: public QMainWindow
	, virtual public IMainWindow
{
	Q_OBJECT
	NON_COPY_MOVABLE(MainWindow)

public:
	MainWindow(const std::shared_ptr<const ILogicFactory>& logicFactory,
	           std::shared_ptr<const IStyleApplierFactory> styleApplierFactory,
	           std::shared_ptr<IUiFactory> uiFactory,
	           std::shared_ptr<ISettings> settings,
	           std::shared_ptr<ICollectionController> collectionController,
	           std::shared_ptr<ICollectionUpdateChecker> collectionUpdateChecker,
	           std::shared_ptr<IParentWidgetProvider> parentWidgetProvider,
	           std::shared_ptr<IAnnotationController> annotationController,
	           std::shared_ptr<AnnotationWidget> annotationWidget,
	           std::shared_ptr<AuthorAnnotationWidget> authorAnnotationWidget,
	           std::shared_ptr<LocaleController> localeController,
	           std::shared_ptr<ILogController> logController,
	           std::shared_ptr<ProgressBar> progressBar,
	           std::shared_ptr<LogItemDelegate> logItemDelegate,
	           std::shared_ptr<ICommandLine> commandLine,
	           std::shared_ptr<ILineOption> lineOption,
	           std::shared_ptr<IDatabaseChecker> databaseChecker,
	           QWidget* parent = nullptr);
	~MainWindow() override;

signals:
	void BookTitleToSearchVisibleChanged() const;

private: // IMainWindow
	void Show() override;

private: // QWidget
	void keyPressEvent(QKeyEvent* event) override;

private slots:
	void OnBooksSearchFilterValueGeometryChanged(const QRect& geometry);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
