#include "ImageUtil.h"

#include <QImage>

namespace HomeCompa::Util
{
QImage HasAlpha(const QImage& image, const char* data)
{
	if (memcmp(data, "\xFF\xD8\xFF\xE0", 4) == 0)
		return image.convertToFormat(QImage::Format_RGB888);

	if (!image.hasAlphaChannel())
		return image.convertToFormat(QImage::Format_RGB888);

	auto argb = image.convertToFormat(QImage::Format_RGBA8888);
	for (int i = 0, h = argb.height(), w = argb.width(); i < h; ++i)
	{
		const auto* pixels = reinterpret_cast<const QRgb*>(argb.constScanLine(i));
		if (std::any_of(pixels, pixels + static_cast<size_t>(w), [](const QRgb pixel) {
				return qAlpha(pixel) < UCHAR_MAX;
			}))
		{
			return argb;
		}
	}

	return image.convertToFormat(QImage::Format_RGB888);
}

}
