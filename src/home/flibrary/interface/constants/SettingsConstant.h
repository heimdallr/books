#pragma once

namespace HomeCompa::Flibrary::Constant::Settings
{

constexpr auto VIEW_MODE_KEY_TEMPLATE       = "ui/%1/Mode";
constexpr auto VIEW_NAVIGATION_KEY_TEMPLATE = "ui/View/Navigation/%1";
constexpr auto FONT_KEY                     = "ui/Font";
constexpr auto FONT_SIZE_KEY                = "ui/Font/pointSizeF";
constexpr auto FONT_SIZE_FAMILY             = "ui/Font/family";
constexpr auto RECENT_NAVIGATION_ID_KEY     = "Collections/%1/Navigation/%2/LastId";

constexpr auto EXPORT_DIALOG_KEY           = "Export";
constexpr auto EXPORT_TEMPLATE_KEY         = "ui/Export/OutputTemplate";
constexpr auto EXPORT_GRAYSCALE_COVER_KEY  = "ui/Export/GrayscaleCover";
constexpr auto EXPORT_GRAYSCALE_IMAGES_KEY = "ui/Export/GrayscaleImages";
constexpr auto EXPORT_REMOVE_COVER_KEY     = "ui/Export/RemoveCover";
constexpr auto EXPORT_REMOVE_IMAGES_KEY    = "ui/Export/RemoveImages";
constexpr auto EXPORT_REPLACE_METADATA_KEY = "ui/Export/ReplaceMetadata";

constexpr auto PERMANENT_LANG_FILTER_KEY         = "ui/permanentLanguageFilter";
constexpr auto PERMANENT_LANG_FILTER_ENABLED_KEY = "ui/permanentLanguageFilterEnabled";
constexpr auto HIDE_SCROLLBARS_KEY               = "ui/hideScrollBars";
constexpr auto SHOW_REMOVED_BOOKS_KEY            = "ui/View/RemovedBooks";

constexpr auto FONT_SIZE_DEFAULT = 9;

constexpr auto OPDS_HOST_KEY              = "opds/host";
constexpr auto OPDS_PORT_KEY              = "opds/port";
constexpr auto OPDS_HOST_DEFAULT          = "Any";
constexpr auto OPDS_PORT_DEFAULT          = 12791;
constexpr auto OPDS_READ_URL_TEMPLATE     = "Preferences/opds/ReadUrlTemplate";
constexpr auto OPDS_AUTOUPDATE_COLLECTION = "Preferences/opds/AutoupdateCollection";
constexpr auto OPDS_AUTH                  = "opds/Authentication";
constexpr auto OPDS_WEB_ENABLED           = "opds/SimpleWebEnabled";
constexpr auto OPDS_REACT_APP_ENABLED     = "opds/ReactAppEnabled";
constexpr auto OPDS_OPDS_ENABLED          = "opds/OpdsEnabled";

constexpr auto LIBRATE_STAR_SYMBOL_KEY            = "ui/Books/LibRate/symbol";
constexpr auto LIBRATE_VIEW_PRECISION_KEY         = "ui/Books/LibRate/precision";
constexpr auto LIBRATE_VIEW_COLORS_KEY            = "ui/Books/LibRate/colors";
constexpr auto LIBRATE_STAR_SYMBOL_DEFAULT        = 0x2B50;
constexpr auto LIBRATE_VIEW_PRECISION_DEFAULT     = -1;
constexpr auto COLLECTIONS                        = "Collections";
constexpr auto DESTRUCTIVE_OPERATIONS_ALLOWED_KEY = "destructiveOperationsAllowed";

constexpr auto PREFER_RELATIVE_PATHS   = "Preferences/RelativePaths";
constexpr auto PREFER_HIDE_TO_TRAY_KEY = "Preferences/HideToTray";

} // namespace HomeCompa::Flibrary::Constant::Settings
