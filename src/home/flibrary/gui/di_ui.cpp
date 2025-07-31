#include "di_ui.h"

#include "Hypodermic/Hypodermic.h"
#include "StyleApplier/StyleApplierFactory.h"
#include "dialogs/AddCollectionDialog.h"
#include "dialogs/ComboBoxTextDialog.h"
#include "dialogs/script/ScriptDialog.h"

#include "LineOption.h"
#include "MainWindow.h"
#include "MigrateWindow.h"
#include "UiFactory.h"

namespace HomeCompa::Flibrary
{

void DiUi(Hypodermic::ContainerBuilder& builder, const std::shared_ptr<Hypodermic::Container>& container)
{
	builder.registerType<AddCollectionDialog>().as<IAddCollectionDialog>();
	builder.registerType<ComboBoxTextDialog>().as<IComboBoxTextDialog>();
	builder.registerType<LineOption>().as<ILineOption>();
	builder.registerType<MainWindow>().as<IMainWindow>().singleInstance();
	builder.registerType<MigrateWindow>().as<IMigrateWindow>();
	builder.registerType<ScriptDialog>().as<IScriptDialog>();

	builder.registerInstanceFactory([&](Hypodermic::ComponentContext& ctx) { return std::make_shared<StyleApplierFactory>(*container, ctx.resolve<ISettings>()); }).as<IStyleApplierFactory>().singleInstance();
	builder.registerInstanceFactory([&](Hypodermic::ComponentContext&) { return std::make_shared<UiFactory>(*container); }).as<IUiFactory>().singleInstance();
}

}
