#include <atlcomcli.h>
#include <comdef.h>

#include "MemExtractCallback.h"
#include "OutMemStream.h"
#include "zip/interface/ProgressCallback.h"
#include "PropVariant.h"

using namespace HomeCompa::ZipDetails::Impl::SevenZip;

namespace {
constexpr auto EMPTY_FILE_ALIAS = "[Content]";
}

CComPtr<MemExtractCallback> MemExtractCallback::Create(CComPtr<IInArchive> archiveHandler, QByteArray & buffer, std::shared_ptr<ProgressCallback> callback, QString password)
{
	return new MemExtractCallback(std::move(archiveHandler), buffer, std::move(callback), std::move(password));
}

MemExtractCallback::MemExtractCallback(CComPtr<IInArchive> archiveHandler, QByteArray & buffer, std::shared_ptr<ProgressCallback> callback, QString password)
	: m_archiveHandler(std::move(archiveHandler))
	, m_buffer(buffer)
	, m_callback(std::move(callback))
	, m_password(std::move(password))
{
}

STDMETHODIMP MemExtractCallback::QueryInterface(REFIID iid, void ** ppvObject)
{
	if (iid == __uuidof(IUnknown))
	{
		*ppvObject = reinterpret_cast<IUnknown *>(this);
		AddRef();
		return S_OK;
	}

	if (iid == IID_IArchiveExtractCallback)
	{
		*ppvObject = static_cast<IArchiveExtractCallback *>(this);
		AddRef();
		return S_OK;
	}

	if (iid == IID_ICryptoGetTextPassword)
	{
		*ppvObject = static_cast<ICryptoGetTextPassword *>(this);
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) MemExtractCallback::AddRef()
{
	return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
}

STDMETHODIMP_(ULONG) MemExtractCallback::Release()
{
	const auto res = static_cast<ULONG>(InterlockedDecrement(&m_refCount));
	if (res == 0)
		delete this;

	return res;
}

// SetTotal is never called for ZIP and 7z formats
STDMETHODIMP MemExtractCallback::SetTotal(const UInt64 /*size*/)
{
	return CheckBreak();
}

STDMETHODIMP MemExtractCallback::SetCompleted(const UInt64 * /*completeValue*/)
{
	//Callback Event calls
	/*
	NB:
	- For ZIP format SetCompleted only called once per 1000 files in central directory and once per 100 in local ones.
	- For 7Z format SetCompleted is never called.
	*/
	return CheckBreak();
}

STDMETHODIMP MemExtractCallback::CheckBreak()
{
	return m_callback->OnCheckBreak() ? E_ABORT : S_OK;
}

STDMETHODIMP MemExtractCallback::GetStream(const UInt32 index, ISequentialOutStream ** outStream, const Int32 askExtractMode)
{
	try
	{
		// Retrieve all the various properties for the file at this index.
		GetPropertyFilePath(index);
		if (askExtractMode != NArchive::NExtract::NAskMode::kExtract)
			return S_OK;

		GetPropertyIsDir(index);
		GetPropertySize(index);
	}
	catch (_com_error & ex)
	{
		return ex.Error();
	}

	if (!m_isDir)
	{
		auto outStreamLoc = OutMemStream::Create(m_buffer);
		m_outMemStream = outStreamLoc;
		*outStream = outStreamLoc.Detach();
	}


	return CheckBreak();
}

STDMETHODIMP MemExtractCallback::PrepareOperation(Int32 /*askExtractMode*/)
{
	return S_OK;
}

STDMETHODIMP MemExtractCallback::SetOperationResult(const Int32 operationResult)
{
	int errors = 0;
	switch (operationResult)
	{
		case NArchive::NExtract::NOperationResult::kOK:
			break;

		default:
		{
			errors++;
			switch (operationResult)
			{
				case NArchive::NExtract::NOperationResult::kUnsupportedMethod:
					break;

				case NArchive::NExtract::NOperationResult::kCRCError:
					break;

				case NArchive::NExtract::NOperationResult::kDataError:
					break;

				default:;
			}
		}
	}

	m_outMemStream.Release();

	if (errors)
	{
		return E_FAIL;
	}

	EmitFileDoneCallback();
	return CheckBreak();
}

STDMETHODIMP MemExtractCallback::CryptoGetTextPassword(BSTR * password)
{
	if (!m_password.isEmpty())
		*password = SysAllocString(m_password.toStdWString().data());

	return S_OK;
}

void MemExtractCallback::GetPropertyFilePath(const UInt32 index)
{
	CPropVariant prop;
	const HRESULT hr = m_archiveHandler->GetProperty(index, kpidPath, &prop);
	if (hr != S_OK)
	{
		_com_issue_error(hr);
	}

	if (prop.vt == VT_EMPTY)
	{
		m_filePath = EMPTY_FILE_ALIAS;
	}
	else if (prop.vt != VT_BSTR)
	{
		_com_issue_error(E_FAIL);
	}
	else
	{
		m_filePath = QString::fromStdWString(prop.bstrVal);
	}
}

void MemExtractCallback::GetPropertyIsDir(const UInt32 index)
{
	CPropVariant prop;
	const HRESULT hr = m_archiveHandler->GetProperty(index, kpidIsDir, &prop);
	if (hr != S_OK)
	{
		_com_issue_error(hr);
	}

	if (prop.vt == VT_EMPTY)
	{
		m_isDir = false;
	}
	else if (prop.vt != VT_BOOL)
	{
		_com_issue_error(E_FAIL);
	}
	else
	{
		m_isDir = prop.boolVal != VARIANT_FALSE;
	}
}

void MemExtractCallback::GetPropertySize(const UInt32 index)
{
	CPropVariant prop;
	const HRESULT hr = m_archiveHandler->GetProperty(index, kpidSize, &prop);
	if (hr != S_OK)
	{
		_com_issue_error(hr);
	}

	switch (prop.vt)
	{
		case VT_EMPTY:
			m_hasNewFileSize = false;
			return;
		case VT_UI1:
			m_newFileSize = prop.bVal;
			break;
		case VT_UI2:
			m_newFileSize = prop.uiVal;
			break;
		case VT_UI4:
			m_newFileSize = prop.ulVal;
			break;
		case VT_UI8:
			m_newFileSize = (UInt64)prop.uhVal.QuadPart;
			break;
		default:
			_com_issue_error(E_FAIL);
	}

	m_hasNewFileSize = true;
}

void MemExtractCallback::EmitDoneCallback() const
{
	m_callback->OnDone();
}

void MemExtractCallback::EmitFileDoneCallback(const QString & path) const
{
	m_callback->OnProgress(m_newFileSize);
	m_callback->OnFileDone(path, m_newFileSize);
}
