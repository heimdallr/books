#include <QImage>

#include <jxl/encode.h>
#include <jxl/encode_cxx.h>
#include <jxl/thread_parallel_runner.h>
#include <jxl/thread_parallel_runner_cxx.h>

#include "jxl.h"
#include "log.h"

namespace HomeCompa::JXL
{

QByteArray Encode(const QImage& image, int quality)
{
	std::vector<uint8_t> compressed;

	JxlEncoderPtr enc = JxlEncoderMake(/*memory_manager=*/nullptr);
	JxlThreadParallelRunnerPtr runner = JxlThreadParallelRunnerMake(
		/*memory_manager=*/nullptr,
		JxlThreadParallelRunnerDefaultNumWorkerThreads());
	if (JXL_ENC_SUCCESS != JxlEncoderSetParallelRunner(enc.get(), JxlThreadParallelRunner, runner.get()))
	{
		PLOGE << "JxlEncoderSetParallelRunner failed";
		return {};
	}

	const auto imagePixelFormat = image.pixelFormat();

	JxlPixelFormat pixel_format { imagePixelFormat.channelCount(), JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, static_cast<size_t>(image.bytesPerLine()) };

	JxlBasicInfo basic_info;
	JxlEncoderInitBasicInfo(&basic_info);
	basic_info.xsize = image.width();
	basic_info.ysize = image.height();
	basic_info.bits_per_sample = 8;
	basic_info.exponent_bits_per_sample = 0;
	basic_info.uses_original_profile = JXL_FALSE;
	basic_info.num_extra_channels = imagePixelFormat.alphaUsage() == QPixelFormat::AlphaUsage::UsesAlpha ? 1 : 0;
	if (JXL_ENC_SUCCESS != JxlEncoderSetBasicInfo(enc.get(), &basic_info))
	{
		PLOGE << "JxlEncoderSetBasicInfo failed";
		return {};
	}

	JxlColorEncoding color_encoding = {};
	JXL_BOOL is_gray = TO_JXL_BOOL(pixel_format.num_channels < 3);
	JxlColorEncodingSetToSRGB(&color_encoding, is_gray);
	if (JXL_ENC_SUCCESS != JxlEncoderSetColorEncoding(enc.get(), &color_encoding))
	{
		PLOGE << "JxlEncoderSetColorEncoding failed";
		return {};
	}

	JxlEncoderFrameSettings* frame_settings = JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
	if (!frame_settings)
	{
		PLOGE << "JxlEncoderFrameSettingsCreate failed";
		return {};
	}

	if (quality < 0)
		quality = 70;

	if (JXL_ENC_SUCCESS != JxlEncoderSetFrameDistance(frame_settings, JxlEncoderDistanceFromQuality(static_cast<float>(quality))))
	{
		PLOGE << "JxlEncoderSetFrameDistance failed";
		return {};
	}

	if (JXL_ENC_SUCCESS != JxlEncoderAddImageFrame(frame_settings, &pixel_format, image.constBits(), image.height() * image.bytesPerLine()))
	{
		PLOGE << "JxlEncoderAddImageFrame failed";
		return {};
	}
	JxlEncoderCloseInput(enc.get());

	compressed.resize(16 * 1024);
	uint8_t* next_out = compressed.data();
	size_t avail_out = compressed.size() - (next_out - compressed.data());
	JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
	while (process_result == JXL_ENC_NEED_MORE_OUTPUT)
	{
		process_result = JxlEncoderProcessOutput(enc.get(), &next_out, &avail_out);
		if (process_result == JXL_ENC_NEED_MORE_OUTPUT)
		{
			size_t offset = next_out - compressed.data();
			compressed.resize(compressed.size() * 2);
			next_out = compressed.data() + offset;
			avail_out = compressed.size() - offset;
		}
	}
	compressed.resize(next_out - compressed.data());
	if (JXL_ENC_SUCCESS != process_result)
	{
		PLOGE << "JxlEncoderProcessOutput failed";
		return {};
	}

	return QByteArray { reinterpret_cast<char*>(compressed.data()), static_cast<qsizetype>(compressed.size()) };
}

} // namespace HomeCompa::JXL
