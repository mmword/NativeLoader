#pragma once
#include "stdafx.h"

enum ErrorCode
{
	EC_OK,
	EC_NotInitialized,
	EC_AlreadyExist,
	EC_WrongParameter,
	EC_NotExist,
	EC_NotEnabled,
	EC_AlreadyProcessed
};

enum OperationStatus
{
	OS_NotInitialized,
	OS_PrepareToDownload,
	OS_Processed,
	OS_Abort,
	OS_Terminate,
	OS_Fail,
	OS_Finished
};

enum RequestAtomicFlags
{
	R_None = 0,
	R_Finished = 1 << 0,
	R_FullUrlPath = 1 << 1
};

#ifdef UNICODE
typedef std::wstring String;
typedef wchar_t *RString;
typedef wchar_t Symbol;
#define STR(quote) L##quote
#else
typedef std::string String;
typedef char *RString;
typedef char Symbol;
#define STR(quote) quote
#endif

typedef std::vector<String> *pvec_str;

extern void WorkedThread(void *data);

class iThread
{
public:
	virtual  ~iThread() {}
	virtual const void Sleep(uint32_t milliseconds) const = 0;
};


class DownloadRequest
{
	uint32_t id;
	uint32_t max_attempts;
	uint32_t current_attempt;
	std::atomic_int downloaded_bytes;
	uint32_t sizeOfDownloadFile;
	uint32_t host_index;
	String onlyFilename;
	pvec_str filePaths;
	pvec_str httpHosts;
	String last_error;
	OperationStatus status;
	std::atomic_int atomicFlags;
	bool manageAttempts;
	uint32_t priority;
	friend void WorkedThread(void *data);
	friend bool ManageRequestError(DownloadRequest *req);
public:
	DownloadRequest() : id(0),max_attempts(0), current_attempt(0), downloaded_bytes(0), sizeOfDownloadFile(0), host_index(0),
		onlyFilename(), filePaths(nullptr), httpHosts(nullptr), last_error(), status(OS_NotInitialized), atomicFlags(0), manageAttempts(true), priority(0){}
	DownloadRequest(pvec_str pfilePaths, pvec_str phosts, uint32_t pid, uint32_t pmax_attempts, const String &pfilename,uint32_t dpriority = 0, OperationStatus initiateStatus = OS_PrepareToDownload)
	{
		id = pid;
		max_attempts = pmax_attempts;
		current_attempt = 0;
		downloaded_bytes = 0;
		host_index = 0;
		sizeOfDownloadFile = 0;
		onlyFilename = pfilename;
		filePaths = pfilePaths;
		httpHosts = phosts;
		last_error.clear();
		status = initiateStatus;
		atomicFlags = 0;
		manageAttempts = true;
		priority = dpriority;
	}
	~DownloadRequest()
	{
		id = 0;
	}

	void ChangeToAbort()
	{
		status = OS_Abort;
	}

	void ChangeToTerminate()
	{
		status = OS_Terminate;
	}

	OperationStatus GetStatus() const
	{
		return status;
	}

	uint32_t GetID() const
	{
		return id;
	}

	uint32_t GetPriority() const
	{
		return priority;
	}

	const String *GetLastError() const
	{
		return &last_error;
	}

	const String *GetCurrentHost() const
	{
		return &httpHosts->at(host_index);
	}

	const String *GetDestinationFolder() const
	{
		return &filePaths->at(0);
	}

	const String *GetFileName() const
	{
		return &onlyFilename;
	}

	String GetCurrentHostFileAsNewString(const RString separator)
	{
		String s(httpHosts->at(host_index));
		s.append(separator);
		s.append(onlyFilename);
		return s;
	}

	String GetDestinationFileAsNewString(const RString separator)
	{
		String s(filePaths->at(0));
		s.append(separator);
		s.append(onlyFilename);
		return s;
	}

	String MakeDestinationFileFromUrl(const RString file_separator,Symbol url_separator, const String *url)
	{
		int idx = url->find_last_of(url_separator);
		if (idx != String::npos)
		{
			String s(filePaths->at(0));
			s.append(file_separator);
			s.append(url->substr(idx+1));
			return s;
		}
		return STR("");
	}

	String GetDestinationDirectory()
	{
		return filePaths->at(0);
	}

	uint32_t GetCurrentHostIndex() const
	{
		return host_index;
	}

	void SetPriority(uint32_t npriority)
	{
		priority = npriority;
	}

	bool IncreaseAttempt()
	{
		if (current_attempt < max_attempts)
		{
			current_attempt++;
			return true;
		}
		return false;
	}

	bool MoveToNextHostAndResetAttempts()
	{
		if (!httpHosts->empty())
		{
			if (host_index < (httpHosts->size() - 1))
			{
				host_index++;
				current_attempt = 0;
				return true;
			}
		}
		return false;
	}

	void WriteError(const String &str)
	{
		last_error.append(str);
	}

	void DontManageAttempts()
	{
		manageAttempts = false;
	}

	void ResetNumberOfDownloadedBytes()
	{
		downloaded_bytes = 0;
	}

	void AddBytes(uint32_t numDownloaded)
	{
		downloaded_bytes += numDownloaded;
	}

	uint32_t GetNumberOfDwonloadedBytes() const
	{
		return downloaded_bytes;
	}

	void SetSizeOfDownloadFile(uint32_t size)
	{
		sizeOfDownloadFile = size;
	}

	const uint32_t GetSizeOfDownloadFile() const
	{
		return sizeOfDownloadFile;
	}

	const RequestAtomicFlags GetAtomicFlags() const
	{
		return (RequestAtomicFlags)atomicFlags.load();
	}

	const void SetAtomicFlags(RequestAtomicFlags flags)
	{
		atomicFlags.store(flags);
	}

};