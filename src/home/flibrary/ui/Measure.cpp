#include "Measure.h"

#include "interface/constants/Localization.h"

namespace HomeCompa::Flibrary::Measure {

namespace {

constexpr auto CONTEXT = "Measure";
constexpr std::pair<qulonglong, const char *> g_sizes[]
{
	{ 1024 * 1024 * 1024, QT_TRANSLATE_NOOP("Measure", "%1 Gb") },
	{        1024 * 1024, QT_TRANSLATE_NOOP("Measure", "%1 Mb") },
	{               1024, QT_TRANSLATE_NOOP("Measure", "%1 Kb") },
	{                  1, QT_TRANSLATE_NOOP("Measure", "%1 b") },
};

}

QString GetSize(const qulonglong size)
{
	const auto it = std::ranges::find_if(std::as_const(g_sizes), [size = size + 1] (const auto & item)
	{
		return size >= item.first;
	});
	assert(it != std::cend(g_sizes));

	return Loc::Tr(CONTEXT, it->second).arg(static_cast<double>(size) / static_cast<double>(it->first), 0, 'f', 1);
}

}
