#include "ArchiveOpenCallback.h"

using namespace HomeCompa::ZipDetails::SevenZip;

CComPtr<ArchiveOpenCallback> ArchiveOpenCallback::Create(QString password)
{
	return new ArchiveOpenCallback(std::move(password));
}

ArchiveOpenCallback::ArchiveOpenCallback(QString password)
	: m_password(std::move(password))
{
}

STDMETHODIMP ArchiveOpenCallback::QueryInterface(REFIID iid, void** ppvObject) //-V835
{
	if (iid == __uuidof(IUnknown))
	{
		*ppvObject = reinterpret_cast<IUnknown*>(this);
		AddRef();
		return S_OK;
	}

	if (iid == IID_IArchiveOpenCallback)
	{
		*ppvObject = static_cast<IArchiveOpenCallback*>(this);
		AddRef();
		return S_OK;
	}

	if (iid == IID_ICryptoGetTextPassword)
	{
		*ppvObject = static_cast<ICryptoGetTextPassword*>(this);
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) ArchiveOpenCallback::AddRef()
{
	return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
}

STDMETHODIMP_(ULONG) ArchiveOpenCallback::Release()
{
	const auto res = static_cast<ULONG>(InterlockedDecrement(&m_refCount));
	if (res == 0)
		delete this;

	return res;
}

STDMETHODIMP ArchiveOpenCallback::SetTotal(const UInt64* /*files*/, const UInt64* /*bytes*/)
{
	return S_OK;
}

STDMETHODIMP ArchiveOpenCallback::SetCompleted(const UInt64* /*files*/, const UInt64* /*bytes*/)
{
	return S_OK;
}

STDMETHODIMP ArchiveOpenCallback::CryptoGetTextPassword(BSTR* password)
{
	if (!m_password.isEmpty())
		*password = SysAllocString(m_password.toStdWString().data());

	return S_OK;
}
