#pragma once

namespace HomeCompa::Flibrary::Constant::Settings {

constexpr auto VIEW_MODE_KEY_TEMPLATE = "ui/%1/Mode";
constexpr auto FONT_KEY = "ui/Font";
constexpr auto FONT_SIZE_KEY = "ui/Font/pointSizeF";
constexpr auto FONT_SIZE_FAMILY = "ui/Font/family";
constexpr auto RECENT_NAVIGATION_ID_KEY = "Collections/%1/Navigation/%2/LastId";
constexpr auto EXPORT_TEMPLATE_KEY = "ui/Export/OutputTemplate";
constexpr auto EXPORT_TEMPLATE_DEFAULT = "%user_destination_folder%/%author%/[%series%/[%seq_number%-]]%title%.%file_ext%";

constexpr auto FONT_SIZE_DEFAULT = 9;
}
