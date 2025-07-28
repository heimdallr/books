#include <QImage>

#include <jxl/decode.h>
#include <jxl/decode_cxx.h>
#include <jxl/resizable_parallel_runner.h>
#include <jxl/resizable_parallel_runner_cxx.h>

#include "jxl.h"
#include "log.h"

namespace HomeCompa::JXL
{

QImage Decode(const QByteArray& bytes)
{
	QImage image;
	auto runner = JxlResizableParallelRunnerMake(nullptr);

	auto dec = JxlDecoderMake(nullptr);
	if (JXL_DEC_SUCCESS != JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE))
	{
		PLOGE << "JxlDecoderSubscribeEvents failed";
		return {};
	}

	if (JXL_DEC_SUCCESS != JxlDecoderSetParallelRunner(dec.get(), JxlResizableParallelRunner, runner.get()))
	{
		PLOGE <<  "JxlDecoderSetParallelRunner failed";
		return {};
	}

	JxlBasicInfo info;
	JxlPixelFormat format;

	JxlDecoderSetInput(dec.get(), reinterpret_cast<const uint8_t*>(bytes.constData()), static_cast<size_t>(bytes.size()));
	JxlDecoderCloseInput(dec.get());

	while (true)
	{
		switch (const auto status = JxlDecoderProcessInput(dec.get()); status)  // NOLINT(clang-diagnostic-switch-enum)
		{
			case JXL_DEC_ERROR:
				PLOGE << "Decoder error";
				return {};

			case JXL_DEC_NEED_MORE_INPUT:
				PLOGE << "Error, already provided all input";
				return {};

			case JXL_DEC_BASIC_INFO:
				if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec.get(), &info))
				{
					PLOGE << "JxlDecoderGetBasicInfo failed";
					return {};
				}

				image = QImage(static_cast<int>(info.xsize), static_cast<int>(info.ysize), info.num_extra_channels ? QImage::Format_RGBA8888 : QImage::Format_RGB888);
				format = JxlPixelFormat { image.pixelFormat().channelCount(), JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, static_cast<size_t>(image.bytesPerLine()) };

				JxlResizableParallelRunnerSetThreads(runner.get(), JxlResizableParallelRunnerSuggestThreads(info.xsize, info.ysize));
				break;

			case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
			{
				auto imageDataSize = static_cast<size_t>(image.height() * image.bytesPerLine());
				size_t buffer_size;
				if (JXL_DEC_SUCCESS != JxlDecoderImageOutBufferSize(dec.get(), &format, &buffer_size))
				{
					PLOGE << "JxlDecoderImageOutBufferSize failed";
					return {};
				}
				if (buffer_size > imageDataSize)
				{
					PLOGE << QString("Invalid out buffer size %d vs %d").arg(buffer_size).arg(imageDataSize);
					return {};
				}

				if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec.get(), &format, image.bits(), imageDataSize))
				{
					PLOGE << "JxlDecoderSetImageOutBuffer failed";
					return {};
				}
				break;
			}

			case JXL_DEC_FULL_IMAGE:
				break;

			case JXL_DEC_SUCCESS:
				return image;

			default:
				PLOGE << "Unknown decoder status";
				return {};
		}
	}
}

}
