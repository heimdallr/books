#pragma once

namespace HomeCompa::Util
{

class LogConsoleFormatter
{
public:
	[[maybe_unused]] static plog::util::nstring format(const plog::Record& record)
	{
		tm t {};
		plog::util::localtime_s(&t, &record.getTime().time);

		plog::util::nostringstream ss;
		ss << severityToString(record.getSeverity())[0] << PLOG_NSTR(" ");
		ss << std::setfill(PLOG_NSTR('0')) << std::setw(2) << t.tm_hour << PLOG_NSTR(":") << std::setfill(PLOG_NSTR('0')) << std::setw(2) << t.tm_min << PLOG_NSTR(":") << std::setfill(PLOG_NSTR('0'))
		   << std::setw(2) << t.tm_sec << PLOG_NSTR(".") << std::setfill(PLOG_NSTR('0')) << std::setw(3) << static_cast<int>(record.getTime().millitm) << PLOG_NSTR(" ");
		ss << PLOG_NSTR("[") << std::hex << std::setw(4) << std::setfill(PLOG_NSTR('0')) << std::right << (record.getTid() & 0xFFFF) << PLOG_NSTR("] ");
		ss << record.getMessage() << PLOG_NSTR("\n");

		return ss.str();
	}
};

}
