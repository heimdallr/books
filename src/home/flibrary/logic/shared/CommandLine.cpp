#include "CommandLine.h"

#include <QCommandLineParser>

#include "interface/constants/Localization.h"

#include "config/version.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto CONTEXT = "CommandLine";
constexpr auto POSITIONAL_ARGUMENT_DESCRIPTION = QT_TRANSLATE_NOOP("CommandLine", "Input file");
constexpr auto APPLICATION_DESCRIPTION = QT_TRANSLATE_NOOP("CommandLine", "%1: e-book cataloger");
TR_DEF
}

struct CommandLine::Impl
{
	std::filesystem::path inpx;
};

CommandLine::CommandLine()
{
	QCommandLineParser parser;
	parser.setApplicationDescription(Tr(APPLICATION_DESCRIPTION).arg(COMPANY_ID));
	parser.addHelpOption();
	parser.addVersionOption();
	parser.addPositionalArgument("file", Tr(POSITIONAL_ARGUMENT_DESCRIPTION));

	parser.process(QCoreApplication::arguments());
	const auto positionalArguments = parser.positionalArguments();

	if (positionalArguments.empty())
		return;

	static bool firstStart = true;
	if (!firstStart)
		return;

	firstStart = false;
	if (const std::filesystem::path file = positionalArguments.front().toStdWString(); file.extension() == ".inpx")
		m_impl->inpx = file.parent_path().make_preferred();
}

CommandLine::~CommandLine() = default;

const std::filesystem::path& CommandLine::GetInpxDir() const noexcept
{
	return m_impl->inpx;
}
