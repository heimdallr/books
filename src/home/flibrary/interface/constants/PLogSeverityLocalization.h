#pragma once

#include <qttranslation.h>

#include <QColor>

#include <plog/Severity.h>

namespace HomeCompa::Flibrary
{

static_assert(plog::Severity::none == 0);
static_assert(plog::Severity::fatal == 1);
static_assert(plog::Severity::error == 2);
static_assert(plog::Severity::warning == 3);
static_assert(plog::Severity::info == 4);
static_assert(plog::Severity::debug == 5);
static_assert(plog::Severity::verbose == 6);

constexpr auto NONE  = QT_TRANSLATE_NOOP("Logging", "NONE");
constexpr auto FATAL = QT_TRANSLATE_NOOP("Logging", "FATAL");
constexpr auto ERROR = QT_TRANSLATE_NOOP("Logging", "ERROR");
constexpr auto WARN  = QT_TRANSLATE_NOOP("Logging", "WARN");
constexpr auto INFO  = QT_TRANSLATE_NOOP("Logging", "INFO");
constexpr auto DEBUG = QT_TRANSLATE_NOOP("Logging", "DEBUG");
constexpr auto VERB  = QT_TRANSLATE_NOOP("Logging", "VERB");

constexpr std::pair<const char*, QColor> SEVERITIES[] {
	{  NONE, QColor(0xff, 0xff, 0xff) },
    { FATAL, QColor(0x99, 0x00, 0x00) },
    { ERROR, QColor(0xff, 0x00, 0x00) },
    {  WARN, QColor(0xcc, 0x00, 0xcc) },
	{  INFO, QColor(0x00, 0x00, 0x00) },
    { DEBUG, QColor(0xa0, 0xa0, 0xa0) },
    {  VERB, QColor(0xe0, 0xe0, 0xe0) },
};

constexpr std::pair<const char*, QColor> SEVERITIES_DARK[] {
	{  NONE, QColor(0x00, 0x00, 0x00) },
    { FATAL, QColor(0x99, 0x00, 0x00) },
    { ERROR, QColor(0xff, 0x00, 0x00) },
    {  WARN, QColor(0xcc, 0xcc, 0x00) },
	{  INFO, QColor(0xff, 0xff, 0xff) },
    { DEBUG, QColor(0x90, 0x90, 0x90) },
    {  VERB, QColor(0x60, 0x60, 0x60) },
};

}
