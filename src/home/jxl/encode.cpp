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

	const JxlEncoderPtr              enc    = JxlEncoderMake(/*memory_manager=*/nullptr);
	const JxlThreadParallelRunnerPtr runner = JxlThreadParallelRunnerMake(
		/*memory_manager=*/nullptr,
		JxlThreadParallelRunnerDefaultNumWorkerThreads()
	);
	if (JXL_ENC_SUCCESS != JxlEncoderSetParallelRunner(enc.get(), JxlThreadParallelRunner, runner.get()))
	{
		PLOGE << "JxlEncoderSetParallelRunner failed";
		return {};
	}

	JxlEncoderFrameSettings* frameSettings = JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
	if (!frameSettings)
	{
		PLOGE << "JxlEncoderFrameSettingsCreate failed";
		return {};
	}

	if (quality < 0)
		quality = 70;
	const auto distance = JxlEncoderDistanceFromQuality(static_cast<float>(quality));
	if (JXL_ENC_SUCCESS != JxlEncoderSetFrameDistance(frameSettings, distance))
	{
		PLOGE << "JxlEncoderSetFrameDistance failed";
		return {};
	}

	if (JXL_ENC_SUCCESS != JxlEncoderUseContainer(enc.get(), 0))
	{
		PLOGE << "JxlEncoderUseContainer failed";
		return {};
	}

	if (JXL_ENC_SUCCESS != JxlEncoderSetCodestreamLevel(enc.get(), -1))
	{
		PLOGE << "JxlEncoderSetCodestreamLevel failed";
		return {};
	}

	const auto           imagePixelFormat = image.pixelFormat();
	const JxlPixelFormat pixelFormat { imagePixelFormat.channelCount(), JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, static_cast<size_t>(image.bytesPerLine()) };

	JxlBasicInfo basicInfo;
	JxlEncoderInitBasicInfo(&basicInfo);
	basicInfo.xsize                    = image.width();
	basicInfo.ysize                    = image.height();
	basicInfo.bits_per_sample          = 8;
	basicInfo.exponent_bits_per_sample = 0;
	basicInfo.uses_original_profile    = JXL_FALSE;
	basicInfo.num_color_channels       = imagePixelFormat.colorModel() == QPixelFormat::Grayscale ? 1 : 3;
	basicInfo.num_extra_channels       = imagePixelFormat.alphaUsage() == QPixelFormat::AlphaUsage::UsesAlpha ? 1 : 0;
	if (basicInfo.num_extra_channels != 0)
		basicInfo.alpha_bits = 8;

	if (JXL_ENC_SUCCESS != JxlEncoderSetBasicInfo(enc.get(), &basicInfo))
	{
		PLOGE << "JxlEncoderSetBasicInfo failed";
		return {};
	}

	if (JXL_ENC_SUCCESS != JxlEncoderSetUpsamplingMode(enc.get(), 1, 1))
	{
		PLOGE << "JxlEncoderSetUpsamplingMode() failed";
		return {};
	}

	JxlBitDepth bitDepth { JXL_BIT_DEPTH_FROM_PIXEL_FORMAT, 0, 0 };
	if (JXL_ENC_SUCCESS != JxlEncoderSetFrameBitDepth(frameSettings, &bitDepth))
	{
		PLOGE << "JxlEncoderSetFrameBitDepth() failed";
		return {};
	}

	if (basicInfo.num_extra_channels != 0 && JXL_ENC_SUCCESS != JxlEncoderSetExtraChannelDistance(frameSettings, 0, 0.0f))
	{
		PLOGE << "JxlEncoderSetExtraChannelDistance failed";
		return {};
	}

	JxlColorEncoding colorEncoding = {};
	JXL_BOOL         isGray        = TO_JXL_BOOL(pixelFormat.num_channels < 3);
	JxlColorEncodingSetToSRGB(&colorEncoding, isGray);
	if (JXL_ENC_SUCCESS != JxlEncoderSetColorEncoding(enc.get(), &colorEncoding))
	{
		PLOGE << "JxlEncoderSetColorEncoding failed";
		return {};
	}

	if (JXL_ENC_SUCCESS != JxlEncoderAddImageFrame(frameSettings, &pixelFormat, image.constBits(), image.height() * image.bytesPerLine()))
	{
		PLOGE << "JxlEncoderAddImageFrame failed";
		return {};
	}
	JxlEncoderCloseInput(enc.get());

	compressed.resize(16LL * 1024);
	uint8_t*         nextOut       = compressed.data();
	size_t           availOut      = compressed.size() - (nextOut - compressed.data());
	JxlEncoderStatus processResult = JXL_ENC_NEED_MORE_OUTPUT;
	while (processResult == JXL_ENC_NEED_MORE_OUTPUT)
	{
		processResult = JxlEncoderProcessOutput(enc.get(), &nextOut, &availOut);
		if (processResult == JXL_ENC_NEED_MORE_OUTPUT)
		{
			const auto offset = nextOut - compressed.data();
			compressed.resize(compressed.size() * 2);
			nextOut  = compressed.data() + offset;
			availOut = compressed.size() - offset;
		}
	}
	compressed.resize(nextOut - compressed.data());
	if (JXL_ENC_SUCCESS != processResult)
	{
		PLOGE << "JxlEncoderProcessOutput failed";
		return {};
	}

	return QByteArray { reinterpret_cast<char*>(compressed.data()), static_cast<qsizetype>(compressed.size()) };
}

} // namespace HomeCompa::JXL
