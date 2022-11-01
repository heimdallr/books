#include <mutex>

#include <QTimer>

#include "fnd/FindPair.h"

#include "util/executor.h"
#include "util/executor/factory.h"

#include "ZipLib/ZipFile.h"

#include "SimpleSaxParser/SaxParser.h"

#include "ModelControllers/BooksModelControllerObserver.h"

#include "AnnotationController.h"

namespace HomeCompa::Flibrary {

namespace {

constexpr auto CONTENT_TYPE = "content-type";

enum class XmlParseMode
{
	unknown = -1,
	annotation,
	binary,
};

constexpr std::pair<const char *, XmlParseMode> g_parseModes[]
{
#define ITEM(NAME) { #NAME, XmlParseMode::NAME }
		ITEM(annotation),
		ITEM(binary),
#undef	ITEM
};

std::string cp2utf(const std::string & str)
{
	static const long utf[256] = {
		0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,
		31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,
		59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,
		87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,
		111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,1026,1027,8218,
		1107,8222,8230,8224,8225,8364,8240,1033,8249,1034,1036,1035,1039,1106,8216,8217,
		8220,8221,8226,8211,8212,8250,8482,1113,8250,1114,1116,1115,1119,160,1038,1118,1032,
		164,1168,166,167,1025,169,1028,171,172,173,174,1031,176,177,1030,1110,1169,181,182,
		183,1105,8470,1108,187,1112,1029,1109,1111,1040,1041,1042,1043,1044,1045,1046,1047,
		1048,1049,1050,1051,1052,1053,1054,1055,1056,1057,1058,1059,1060,1061,1062,1063,
		1064,1065,1066,1067,1068,1069,1070,1071,1072,1073,1074,1075,1076,1077,1078,1079,
		1080,1081,1082,1083,1084,1085,1086,1087,1088,1089,1090,1091,1092,1093,1094,1095,
		1096,1097,1098,1099,1100,1101,1102,1103
	};

	std::string res;

	for (const auto s : str)
	{
		const auto c = utf[static_cast<unsigned char>(s)];
		if (c < 0x80)
		{
			res += static_cast<char>(c);
		}
		else if (c < 0x800)
		{
			res += static_cast<char>(c >> 6 | 0xc0);
			res += static_cast<char>((c & 0x3f) | 0x80);
		}
		else if (c < 0x10000)
		{
			res += static_cast<char>(c >> 12 | 0xe0);
			res += static_cast<char>((c >> 6 & 0x3f) | 0x80);
			res += static_cast<char>((c & 0x3f) | 0x80);
		}
	}

	return res;
}

std::string passThru(const std::string & str)
{
	return str;
}

using StringConverter = std::string(*)(const std::string & str);
constexpr std::pair<const char *, StringConverter> g_stringConverters[]
{
	{ "windows-1251", &cp2utf },
	{ "utf-8", &passThru },
};

struct SaxHandler : XSPHandler
{
	std::string annotation;
	std::vector<QString> covers;

private: // XSPHandler
	void OnEncoding([[maybe_unused]] const std::string & name) override
	{
		m_stringConverter = FindSecond(g_stringConverters, name.data(), &passThru, PszComparerCaseInsensitive {});
	}

	void OnElementBegin(const std::string & name) override
	{
		if (name == "p" && m_mode == XmlParseMode::annotation)
		{
			annotation.append("    ");
			return;
		}

		m_mode = FindSecond(g_parseModes, name.data(), XmlParseMode::unknown, PszComparer {});
	}

	void OnElementEnd(const std::string & name) override
	{
		if (name == "p" && m_mode == XmlParseMode::annotation)
		{
			annotation.append("\n\n");
			return;
		}

		m_mode = XmlParseMode::unknown;
	}

	void OnText(const std::string & value) override
	{
		if (m_mode == XmlParseMode::annotation)
			annotation.append(m_stringConverter(value));

		if (m_mode == XmlParseMode::binary)
			ProcessBinary(value);

	}

	void OnAttribute(const std::string & name, const std::string & value) override
	{
		if (m_mode == XmlParseMode::binary && name == CONTENT_TYPE)
			m_binaryContentType = value;
	}

private:
	void ProcessBinary(const std::string & value)
	{
		covers.emplace_back(QString("data:%1;base64,").arg(m_binaryContentType.data()).append(value.data()));
	}

private:
	XmlParseMode m_mode { XmlParseMode::unknown };
	std::string m_binaryContentType;
	StringConverter m_stringConverter { &passThru };
};

}

class AnnotationController::Impl
	: virtual public BooksModelControllerObserver
{
public:
	explicit Impl(const AnnotationController & self)
		: m_self(self)
	{
		m_annotationTimer.setSingleShot(true);
		m_annotationTimer.setInterval(std::chrono::milliseconds(250));
		connect(&m_annotationTimer, &QTimer::timeout, [&] { ExtractAnnotation(); });
	}

	void CoverNext()
	{
		if (m_covers.size() < 2)
			return;

		if (++m_coverIndex >= static_cast<int>(m_covers.size()))
			m_coverIndex = 0;

		emit m_self.CoverChanged();
	}

	void CoverPrev()
	{
		if (m_covers.size() < 2)
			return;

		if (--m_coverIndex < 0)
			m_coverIndex = static_cast<int>(m_covers.size()) - 1;

		emit m_self.CoverChanged();
	}

	void SetRootFolder(std::filesystem::path rootFolder)
	{
		m_rootFolder = std::move(rootFolder);
	}

	const QString & GetAnnotation() const
	{
		std::lock_guard lock(m_guard);
		return m_annotation;
	}

	bool GetHasCover() const
	{
		std::lock_guard lock(m_guard);
		return !m_covers.empty();
	}

	const QString & GetCover() const
	{
		assert(m_coverIndex >= 0 && m_coverIndex < static_cast<int>(std::size(m_covers)));
		return m_covers[m_coverIndex];
	}


private: // BooksModelControllerObserver
	void HandleBookChanged(const std::string & folder, const std::string & file) override
	{
		m_folderTmp = folder;
		m_fileTmp = file;
		m_annotationTimer.start();
	}

private:
	void ExtractAnnotation()
	{
		if (m_folder == m_folderTmp && m_file == m_fileTmp)
			return;

		m_folder = m_folderTmp;
		m_file = m_fileTmp;

		(*m_executor)([&, folder = (m_rootFolder / m_folder).make_preferred(), file = m_file]
		{
			auto stub = [&]
			{
				emit m_self.AnnotationChanged();
				emit m_self.HasCoverChanged();
				emit m_self.CoverChanged();
			};

			{
				std::lock_guard lock(m_guard);
				m_annotation.clear();
				m_covers.clear();
				m_coverIndex = -1;
			}

			if (!exists(folder))
				return stub;

			const auto archive = ZipFile::Open(folder.generic_string());
			if (!archive)
				return stub;

			const auto entry = archive->GetEntry(file);
			if (!entry)
				return stub;

			auto * const stream = entry->GetDecompressionStream();
			if (!stream)
				return stub;

			SaxParser parser;
			SaxHandler handler;
			try
			{
				parser.Parse(stream, &handler);

				std::lock_guard lock(m_guard);
				m_annotation = QString::fromStdString(handler.annotation);
				m_covers = std::move(handler.covers);
				if (!m_covers.empty())
					m_coverIndex = 0;
			}
			catch(SaxParserException &)
			{
			}

			return stub;
		});
	}

private:
	const AnnotationController & m_self;
	PropagateConstPtr<Util::Executor> m_executor { Util::ExecutorFactory::Create(Util::ExecutorImpl::Async) };
	QTimer m_annotationTimer;
	std::filesystem::path m_rootFolder;

	std::string m_folder, m_folderTmp;
	std::string m_file, m_fileTmp;

	mutable std::mutex m_guard;
	QString m_annotation;
	std::vector<QString> m_covers;
	int m_coverIndex { -1 };
};

AnnotationController::AnnotationController()
	: m_impl(*this)
{
}

AnnotationController::~AnnotationController() = default;

void AnnotationController::CoverNext()
{
	m_impl->CoverNext();
}

void AnnotationController::CoverPrev()
{
	m_impl->CoverPrev();
}

BooksModelControllerObserver * AnnotationController::GetBooksModelControllerObserver()
{
	return m_impl.get();
}

void AnnotationController::SetRootFolder(std::filesystem::path rootFolder)
{
	m_impl->SetRootFolder(std::move(rootFolder));
}

const QString & AnnotationController::GetAnnotation() const
{
	return m_impl->GetAnnotation();
}

bool AnnotationController::GetHasCover() const
{
	return m_impl->GetHasCover();
}

const QString & AnnotationController::GetCover() const
{
	return m_impl->GetCover();
}

}
