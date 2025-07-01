#pragma once

#include "win.h"

namespace UnknownImplDetail
{

inline ULONG AddRefImpl(LONG& refCount)
{
	return static_cast<ULONG>(InterlockedIncrement(&refCount));
}

template <typename T>
ULONG ReleaseRefImpl(LONG& refCount, T* obj)
{
	const auto res = static_cast<ULONG>(InterlockedDecrement(&refCount));
	if (res == 0)
		delete obj;

	return res;
}

template <typename T>
HRESULT QueryInterface(REFIID iid, void** ppvObject, REFIID iidObj, T* obj) //-V835
{
	if (iid == __uuidof(IUnknown)) // NOLINT(clang-diagnostic-language-extension-token)
	{
		*ppvObject = reinterpret_cast<IUnknown*>(obj);
		return S_OK;
	}
	if (iid == iidObj)
	{
		*ppvObject = obj;
		return S_OK;
	}
	return E_NOINTERFACE;
}

} // namespace UnknownImplDetail

#define ADD_RELEASE_REF_IMPL                                        \
private:                                                            \
	LONG m_refCount { 0 };                                          \
                                                                    \
public:                                                             \
	ULONG AddRef() override                                         \
	{                                                               \
		return UnknownImplDetail::AddRefImpl(m_refCount);           \
	}                                                               \
	ULONG Release() override                                        \
	{                                                               \
		return UnknownImplDetail::ReleaseRefImpl(m_refCount, this); \
	}                                                               \
                                                                    \
private:

#define UNKNOWN_IMPL(NAME)                                                          \
private:                                                                            \
	HRESULT QueryInterface(REFIID iid, void** ppvObject) override                   \
	{                                                                               \
		return UnknownImplDetail::QueryInterface(iid, ppvObject, IID_##NAME, this); \
	}                                                                               \
	ADD_RELEASE_REF_IMPL                                                            \
private:
