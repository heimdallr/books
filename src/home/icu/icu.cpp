#include "icu.h"

#include <cassert>
#include <format>
#include <stdexcept>

#include <unicode/translit.h>
#include <unicode/ucnv.h>
#include <unicode/unistr.h>
#include <unicode/ustring.h>

#include "fnd/NonCopyMovable.h"

#include "log.h"

void ICU_Transliterate(const char* id, const std::u32string* src, std::u32string* dst)
{
	UErrorCode           status  = U_ZERO_ERROR;
	icu::Transliterator* myTrans = icu::Transliterator::createInstance(id, UTRANS_FORWARD, status);
	assert(myTrans);

	auto icuString = icu::UnicodeString::fromUTF32(reinterpret_cast<const int32_t*>(src->data()), -1);
	myTrans->transliterate(icuString);
	auto buf = std::make_unique_for_overwrite<int32_t[]>(icuString.length() * 4ULL);
	icuString.toUTF32(buf.get(), icuString.length() * 4, status);
	*dst = std::u32string { reinterpret_cast<char32_t*>(buf.get()) };
}

using namespace HomeCompa::ICU;

namespace
{

UConverter* CreateDecoder(const char* id)
{
	UErrorCode status;
	auto*      decoder = ucnv_open(id, &status);
	if (U_FAILURE(status))
		throw std::runtime_error(std::format("cannot open {} decoder", id));

	return decoder;
}

class Decoder final : virtual public IDecoder
{
	NON_COPY_MOVABLE(Decoder)

public:
	explicit Decoder(const char* id)
		: m_decoder { CreateDecoder(id) }
	{
		PLOGV << id << " decoder created";
	}

	~Decoder() override
	{
		ucnv_close(m_decoder);
	}

private: // IDecoder
	std::string Decode(const std::string_view src) const override
	{
		const auto  uStr = DecodeImpl(src);
		std::string result;
		return uStr.toUTF8String(result);
	}

private:
	icu::UnicodeString DecodeImpl(const std::string_view src) const
	{
		std::lock_guard lock(m_guard);

		UErrorCode         status;
		icu::UnicodeString uStr(src.data(), static_cast<int32_t>(src.size()), m_decoder, status);
		if (U_FAILURE(status))
			throw std::runtime_error(std::format("cannot decode {}", src));

		return uStr;
	}

private:
	mutable std::mutex m_guard;
	UConverter*        m_decoder;
};

} // namespace

IDecoder::Ptr IDecoder::Create(const char* id)
{
	return std::make_unique<Decoder>(id);
}
