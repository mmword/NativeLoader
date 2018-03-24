#pragma once
#include "LoaderImp.h"

#include "stdafx.h"
#include "LoaderImp.h"

#ifdef UNITY_WIN
#define API(x) extern "C" __declspec(dllexport) x __stdcall
#else
#define API(x)
#endif

API(int) NLInitialize(int pnumThreads)
{
	return Initialize(pnumThreads);
}

API(void) NLShutdown()
{
	Shutdown();
}

API(void) NLAddHttpUrl(const RString purl)
{
	AddHttpUrl(purl);
}

API(void) NLAddFolder(const RString pfolder)
{
	AddFolder(pfolder);
}

API(int) NLCreateRequest(unsigned int pmaxAttempts,unsigned int priority, const RString pfilename, unsigned int *presID)
{
	DownloadRequest *req = nullptr;
	ErrorCode code = CreateRequest(pmaxAttempts, priority, pfilename, &req);
	if (code == EC_OK)
	{
		*presID = req->GetID();
		return 1;
	}
	return 0;
}

API(int) NLCreateUrlRequest(unsigned int pmaxAttempts, unsigned int priority, const RString url, unsigned int *presID)
{
	DownloadRequest *req = nullptr;
	ErrorCode code = CreateUrlRequest(pmaxAttempts, priority, url, &req);
	if (code == EC_OK)
	{
		*presID = req->GetID();
		return 1;
	}
	return 0;
}

API(int) NLReleaseRequest(unsigned int pid)
{
	return ReleaseRequestByID(pid);
}

API(int) NLSetRequestPriority(unsigned int pid, unsigned int priority)
{
	return SetRequestPriority(pid, priority);
}

API(int) NLReleaseAllRequests()
{
	return ReleaseAllRequests();
}

API(unsigned long long) NLReleaseResult()
{
	if (HasResults())
	{
		unsigned long long mask = 0;
		DequeueAndFreeResult([&mask](DownloadRequest *preq) -> void
		{
			uint32_t x = preq->GetID();
			uint32_t y = preq->GetStatus();
			mask = ((unsigned long long)x) << 32 | y;
		});
		return mask;
	}
	return 0;
}

API(int) NLDequeueResult(unsigned int *id, unsigned int *status, RString last_error,unsigned int maxSze)
{
	if (HasResults())
	{
		DownloadRequest *req = DequeueResult();
		if (req != nullptr)
		{
			*id = req->GetID();
			*status = req->GetStatus();
			size_t _strSize = req->GetLastError()->size();
			if (_strSize > 0)
			{
				if (maxSze < _strSize)
					_strSize = maxSze;
				req->GetLastError()->copy(last_error, _strSize);
			}
			return 1;
		}
	}
	return 0;
}

API(void) NLAbortDownload(unsigned int id)
{
	AbortDownload(id);
}

API(void) NLFreeThreadsIfPossible()
{
	FreeThreadsIfPossible();
}