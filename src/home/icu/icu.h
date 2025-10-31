#pragma once

#include <memory>
#include <string>

#include "export/flicu.h"

namespace HomeCompa::ICU
{
static constexpr auto LIB_NAME           = "flicu";
static constexpr auto TRANSLITERATE_NAME = "ICU_Transliterate";
using TransliterateType                  = void (*)(const char* id, const std::u32string* src, std::u32string* dst);

class IDecoder // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Ptr = std::unique_ptr<IDecoder>;
	FLICU_EXPORT static Ptr Create(const char* id);

public:
	virtual ~IDecoder()                                    = default;
	virtual std::string Decode(std::string_view src) const = 0;
};

}

extern "C" FLICU_EXPORT void ICU_Transliterate(const char* id, const std::u32string* src, std::u32string* dst);
