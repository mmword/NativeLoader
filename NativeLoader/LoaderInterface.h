#pragma once
#include "Defines.h"

class iNativeFileLoader
{
public:
	virtual ~iNativeFileLoader() { }

	virtual void ThreadSafeConstruction(DownloadRequest *request, const iThread *thread) = 0;

	virtual bool Create() = 0;

	virtual bool Process() = 0;  // return true if download still in process

	virtual void Abort() = 0;

	virtual void Release() = 0;
};