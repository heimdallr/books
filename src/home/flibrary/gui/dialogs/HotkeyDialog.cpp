#include "ui_HotkeyDialog.h"

#include "HotkeyDialog.h"

#include "gutil/util.h"
#include "util/GeometryRestorable.h"

using namespace HomeCompa::Flibrary;

namespace
{

constexpr auto CONTEXT         = "HotkeyDialog";
constexpr auto FIELD_WIDTH_KEY = "ui/View/HotkeyDialog/columnWidths";

}

class HotkeyDialog::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(
		QDialog&                             self,
		const IModelProvider&                modelProvider,
		std::shared_ptr<ISettings>           settings,
		std::shared_ptr<IHotkeyManager>      hotkeyManager,
		std::shared_ptr<ItemViewToolTipper>  itemViewToolTipper,
		std::shared_ptr<ScrollBarController> scrollBarController
	)
		: GeometryRestorable(*this, settings, CONTEXT)
		, GeometryRestorableObserver(self)
		, m_self { self }
		, m_settings { std::move(settings) }
		, m_hotkeyManager { std::move(hotkeyManager) }
		, m_model { modelProvider.CreateTreeModel(m_hotkeyManager->GetRootDataItem()) }
		, m_itemViewToolTipper { std::move(itemViewToolTipper) }
		, m_scrollBarController { std::move(scrollBarController) }
	{
		m_ui.setupUi(&m_self);
		m_ui.view->setModel(m_model.get());
		m_ui.view->viewport()->installEventFilter(m_itemViewToolTipper.get());
		m_ui.view->viewport()->installEventFilter(m_scrollBarController.get());
		m_ui.view->header()->setDefaultAlignment(Qt::AlignCenter);

		LoadGeometry();
		Util::LoadHeaderSectionWidth(*m_ui.view->header(), *m_settings, FIELD_WIDTH_KEY);
	}

	~Impl() override
	{
		Util::SaveHeaderSectionWidth(*m_ui.view->header(), *m_settings, FIELD_WIDTH_KEY);
		SaveGeometry();
	}

private:
	QDialog& m_self;

	PropagateConstPtr<ISettings, std::shared_ptr>           m_settings;
	PropagateConstPtr<IHotkeyManager, std::shared_ptr>      m_hotkeyManager;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr>  m_model;
	PropagateConstPtr<ItemViewToolTipper, std::shared_ptr>  m_itemViewToolTipper;
	PropagateConstPtr<ScrollBarController, std::shared_ptr> m_scrollBarController;

	Ui::HotkeyDialog m_ui;
};

HotkeyDialog::HotkeyDialog(
	const std::shared_ptr<IParentWidgetProvider>& parentWidgetProvider,
	const std::shared_ptr<IModelProvider>&        modelProvider,
	std::shared_ptr<ISettings>                    settings,
	std::shared_ptr<IHotkeyManager>               hotkeyManager,
	std::shared_ptr<ItemViewToolTipper>           itemViewToolTipper,
	std::shared_ptr<ScrollBarController>          scrollBarController,
	QWidget*                                      parent
)
	: QDialog(parentWidgetProvider->GetWidget(parent))
	, m_impl(*this, *modelProvider, std::move(settings), std::move(hotkeyManager), std::move(itemViewToolTipper), std::move(scrollBarController))
{
}

HotkeyDialog::~HotkeyDialog() = default;
