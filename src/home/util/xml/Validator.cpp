#include "Validator.h"

#include <QIODevice>
#include <QStringList>

#include <xercesc/dom/DOMError.hpp>
#include <xercesc/dom/DOMErrorHandler.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>
#include <xercesc/dom/DOMLSParser.hpp>
#include <xercesc/dom/DOMLocator.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/framework/Wrapper4InputSource.hpp>
#include <xercesc/parsers/AbstractDOMParser.hpp>

#include "Initializer.h"

using namespace HomeCompa::Util;
namespace xercesc = xercesc_3_3;

namespace
{

class StrX
{
	NON_COPY_MOVABLE(StrX)

public:
	StrX(const XMLCh* const toTranscode)
		: m_fLocalForm { xercesc::XMLString::transcode(toTranscode) }
	{
	}

	~StrX()
	{
		xercesc::XMLString::release(&m_fLocalForm);
	}

	const char* ToString() const
	{
		return m_fLocalForm;
	}

private:
	char* m_fLocalForm;
};

class ErrorHandler : public xercesc::DOMErrorHandler
{
public:
	QString GetErrors() const
	{
		return m_errors.join(",");
	}

private: // xercesc::DOMErrorHandler
	bool handleError(const xercesc::DOMError& domError) override
	{
		QString error = domError.getSeverity() == xercesc::DOMError::DOM_SEVERITY_WARNING ? "Warning " : domError.getSeverity() == xercesc::DOMError::DOM_SEVERITY_ERROR ? "Error " : "Fatal Error ";

		const auto& location = *domError.getLocation();
		error.append(QString("%1, line %2, char %3: %4").arg(StrX(location.getURI()).ToString()).arg(location.getLineNumber()).arg(location.getColumnNumber()).arg(StrX(domError.getMessage()).ToString()));

		m_errors.push_back(std::move(error));

		return true;
	}

private:
	QStringList m_errors;
};

} // namespace

class XmlValidator::Impl
{
private:
	XMLPlatformInitializer m_initializer;
};

XmlValidator::XmlValidator() = default;

XmlValidator::~XmlValidator() = default;

QString XmlValidator::Validate(QIODevice& input) const
{
	static const XMLCh          gLS[]  = { xercesc::chLatin_L, xercesc::chLatin_S, xercesc::chNull };
	xercesc::DOMImplementation* impl   = xercesc::DOMImplementationRegistry::getDOMImplementation(gLS);
	xercesc::DOMLSParser*       parser = impl->createLSParser(xercesc::DOMImplementationLS::MODE_SYNCHRONOUS, nullptr);
	xercesc::DOMConfiguration*  config = parser->getDomConfig();
	config->setParameter(xercesc::XMLUni::fgDOMNamespaces, true);
	config->setParameter(xercesc::XMLUni::fgXercesHandleMultipleImports, true);

	const ErrorHandler errorHandler;
	config->setParameter(xercesc::XMLUni::fgDOMErrorHandler, &errorHandler);

	const auto                         body = input.readAll();
	xercesc::MemBufInputSource         inputSource(reinterpret_cast<const unsigned char*>(body.data()), body.size(), "");
	const xercesc::Wrapper4InputSource domInput(&inputSource, false);

	parser->resetDocumentPool();
	parser->parse(&domInput);
	parser->release();

	return errorHandler.GetErrors();
}
