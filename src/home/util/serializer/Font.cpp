#include "Font.h"

#include <QtGui/QFont>

#include "ISettings.h"

namespace HomeCompa::Util
{

namespace
{
constexpr auto FAMILY              = "family";
constexpr auto POINT_SIZE_F        = "pointSizeF";
constexpr auto PIXEL_SIZE          = "pixelSize";
constexpr auto STYLE_HINT          = "styleHint";
constexpr auto WEIGHT              = "weight";
constexpr auto STYLE               = "style";
constexpr auto UNDERLINE           = "underline";
constexpr auto STRIKE_OUT          = "strikeOut";
constexpr auto FIXED_PITCH         = "fixedPitch";
constexpr auto CAPITALIZATION      = "capitalization";
constexpr auto LETTER_SPACING_TYPE = "letterSpacingType";
constexpr auto LETTER_SPACING      = "letterSpacing";
constexpr auto WORD_SPACING        = "wordSpacing";
constexpr auto STRETCH             = "stretch";
constexpr auto STYLE_STRATEGY      = "styleStrategy";
}

void Serialize(const QFont& font, ISettings& settings)
{
	settings.Set(FAMILY, font.family());
	settings.Set(POINT_SIZE_F, font.pointSizeF());
	if (font.pixelSize() > 0)
		settings.Set(PIXEL_SIZE, font.pixelSize());
	settings.Set(STYLE_HINT, font.styleHint());
	settings.Set(WEIGHT, font.weight());
	settings.Set(STYLE, font.style());
	settings.Set(UNDERLINE, font.underline());
	settings.Set(STRIKE_OUT, font.strikeOut());
	settings.Set(FIXED_PITCH, font.fixedPitch());
	settings.Set(CAPITALIZATION, font.capitalization());
	settings.Set(LETTER_SPACING_TYPE, font.letterSpacingType());
	settings.Set(LETTER_SPACING, font.letterSpacing());
	settings.Set(WORD_SPACING, font.wordSpacing());
	settings.Set(STRETCH, font.stretch());
	settings.Set(STYLE_STRATEGY, font.styleStrategy());
}

void Deserialize(QFont& font, const ISettings& settings)
{
	font.setFamily(settings.Get(FAMILY, font.family()));
	font.setPointSizeF(settings.Get(POINT_SIZE_F, font.pointSizeF()));
	if (const auto pixelSize = settings.Get(PIXEL_SIZE, font.pixelSize()); pixelSize > 0)
		font.setPixelSize(pixelSize);
	font.setStyleHint(settings.Get(STYLE_HINT, font.styleHint()));
	font.setWeight(settings.Get(WEIGHT, font.weight()));
	font.setStyle(settings.Get(STYLE, font.style()));
	font.setUnderline(settings.Get(UNDERLINE, font.underline()));
	font.setStrikeOut(settings.Get(STRIKE_OUT, font.strikeOut()));
	font.setFixedPitch(settings.Get(FIXED_PITCH, font.fixedPitch()));
	font.setCapitalization(settings.Get(CAPITALIZATION, font.capitalization()));
	font.setLetterSpacing(settings.Get(LETTER_SPACING_TYPE, font.letterSpacingType()), settings.Get(LETTER_SPACING, font.letterSpacing()));
	font.setWordSpacing(settings.Get(WORD_SPACING, font.wordSpacing()));
	font.setStretch(settings.Get(STRETCH, font.stretch()));
	font.setStyleStrategy(settings.Get(STYLE_STRATEGY, font.styleStrategy()));
}

} // namespace HomeCompa::Util
