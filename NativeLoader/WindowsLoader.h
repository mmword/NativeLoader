#pragma once

#include "PlatformBase.h"

#ifdef UNITY_WIN

#include "Defines.h"
#include "LoaderInterface.h"
#include <Windows.h>
#include <wininet.h>
#include <strsafe.h>

#pragma comment(lib,"wininet.lib")

#ifdef UNICODE
const RString FileSeparator = L"\\";
const RString UrlSeparator = L"/";
#else
const RString Separator = "\\";
const RString UrlSeparator = "/";
#endif

const Symbol CUrlSeparator = '/';
const Symbol CFileSeparator = '\\';

#define MAX_OPEN_INTERNAL_ATTEMPTS 3
#define SINGLE_SESSION
#define BUFFER_SIZE 4096

class InternetSession
{
	HINTERNET internetHandle;
#ifdef SINGLE_SESSION
	std::mutex hMutex;
#endif
public:

	~InternetSession()
	{
		if (internetHandle != nullptr)
		{
			InternetCloseHandle(internetHandle);
			internetHandle = nullptr;
		}
	}

#ifdef SINGLE_SESSION
	HINTERNET GetSession()
	{
		if (internetHandle == nullptr)
		{
			{
				std::lock_guard<std::mutex> lc(hMutex);
				if (internetHandle == nullptr)
				{
					internetHandle = InternetOpen(STR("Unity Native Loader (windows)"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, INTERNET_FLAG_KEEP_CONNECTION);
				}
			}
		}
		return internetHandle;
	}
#else
	HINTERNET GetSession()
	{
		if (internetHandle == nullptr)
		{
			internetHandle = InternetOpen(STR("Unity Native Loader (windows)"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
		}
		return internetHandle;
	}
#endif

};

#ifdef SINGLE_SESSION
static InternetSession Session;
#endif

class WinInetWindowsLoader : public iNativeFileLoader
{
	DownloadRequest *request;
	const iThread *thread;
	HANDLE fileHandle;
	HINTERNET urlHandle;
	BYTE buffer[BUFFER_SIZE];
	String fullFilePath;
	String fullUrlPath;
	String destDirectory;

#ifndef SINGLE_SESSION
	InternetSession Session;
#endif

	String GetLastErrorString(LPTSTR lpszFunction)
	{
		// Retrieve the system error message for the last-error code
		LPVOID lpMsgBuf;
		LPVOID lpDisplayBuf;
		DWORD dw = GetLastError();
		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			dw,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf,
			0, NULL);
		// Display the error message and exit the process
		lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
		StringCchPrintf((LPTSTR)lpDisplayBuf, LocalSize(lpDisplayBuf) / sizeof(TCHAR), TEXT("%s failed with error %d: %s"), lpszFunction, dw, lpMsgBuf);
		String res((RString)lpDisplayBuf);
		LocalFree(lpMsgBuf);
		LocalFree(lpDisplayBuf);
		return res;
	}

public:

	WinInetWindowsLoader() : request(nullptr), fileHandle(nullptr),urlHandle(nullptr) {}
	~WinInetWindowsLoader() { Release(); }

	virtual void ThreadSafeConstruction(DownloadRequest *request, const iThread *thread)
	{
		this->request = request;
		this->thread = thread;
		if ((request->GetAtomicFlags() & R_FullUrlPath) > 0)
		{
			fullUrlPath = String(request->GetFileName()->c_str());
			fullFilePath = request->MakeDestinationFileFromUrl(FileSeparator, CUrlSeparator, request->GetFileName());
		}
		else
		{
			fullUrlPath = request->GetCurrentHostFileAsNewString(UrlSeparator);
			fullFilePath = request->GetDestinationFileAsNewString(FileSeparator);
		}
		destDirectory = request->GetDestinationDirectory();
	}

	virtual bool Create()
	{
		if (request != nullptr)
		{
			request->ResetNumberOfDownloadedBytes();

			// TODO : may use attempts here
			HINTERNET internetHandle = Session.GetSession();
			if (internetHandle == NULL)
			{
				request->WriteError(STR("cant open session"));
				return false;
			}

			int internal_attempts = MAX_OPEN_INTERNAL_ATTEMPTS;
			while (internal_attempts-- > 0)
			{
				urlHandle = InternetOpenUrl(internetHandle, fullUrlPath.c_str(), NULL, 0, INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_RELOAD, 0);
				if (urlHandle != nullptr)
					break;
				thread->Sleep(1000);
			}

			if (urlHandle == nullptr)
			{
				request->WriteError(STR("cant open url : "));
				request->WriteError(fullUrlPath);
				request->WriteError(GetLastErrorString(STR("InternetOpenUrl")));
				Release();
				return false;
			}

			int statusCode;
			char responseText[256]; // change to wchar_t for unicode
			DWORD responseTextSize = sizeof(responseText);

			if (HttpQueryInfoA(urlHandle, HTTP_QUERY_STATUS_CODE, &responseText, &responseTextSize, NULL)) // HTTP_QUERY_FLAG_NUMBER is not work well
			{
				statusCode = atoi(responseText);
				if (statusCode != HTTP_STATUS_OK)
				{
					Symbol buff[260];
					wsprintf(buff, STR("url %s return wrong code : %d"), fullUrlPath.c_str(), statusCode);
					request->WriteError(buff);
					Release();
					return false;
				}
			}

			if (HttpQueryInfoA(urlHandle, HTTP_QUERY_CONTENT_LENGTH, &responseText, &responseTextSize, NULL)) // HTTP_QUERY_FLAG_NUMBER is not work well
			{
				DWORD Size = atoi(responseText);
				if(Size > 0)
					request->SetSizeOfDownloadFile(Size);
			}

			if (CreateDirectory(destDirectory.c_str(), NULL) || ERROR_ALREADY_EXISTS == GetLastError())
			{
				fileHandle = CreateFile(fullFilePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if (fileHandle == INVALID_HANDLE_VALUE)
				{
					request->WriteError(STR("cant create session fror write file : "));
					request->WriteError(fullFilePath);
					request->WriteError(GetLastErrorString(STR("CreateFile")));
					Release();
					return false;
				}
			}
			else
			{
				request->WriteError(STR("cant create directory for files destination : "));
				request->WriteError(destDirectory);
				Release();
				return false;
			}

			return true;
		}
		return false;
	}

	virtual bool Process()
	{
		if (urlHandle != nullptr && fileHandle != nullptr)
		{
			DWORD numBytesRead = 0;
			if (InternetReadFile(urlHandle, buffer, sizeof(buffer), &numBytesRead))
			{
				if (numBytesRead > 0)
				{
					DWORD numberBytesWritten = 0;
					if(WriteFile(fileHandle, buffer, numBytesRead, &numberBytesWritten, NULL))
						request->AddBytes(numberBytesWritten);
					else
					{
						request->WriteError(STR("file write return fail result"));
						request->WriteError(GetLastErrorString(STR("WriteFile")));
						return false;
					}
				}
				else
				{
					Release();
					return false;
				}
			}
			else
			{
				request->WriteError(STR("internet read return fail result"));
				request->WriteError(GetLastErrorString(STR("InternetReadFile")));
				return false;
			}
		}
		return true;
	}

	virtual void Abort()
	{
		bool hasFile = fileHandle != nullptr;
		Release();
		if (hasFile)
		{
			DeleteFile(fullFilePath.c_str());
		}
	}

	virtual void Release()
	{
		if (urlHandle != nullptr)
		{
			InternetCloseHandle(urlHandle);
			urlHandle = nullptr;
		}
		if (fileHandle != nullptr)
		{
			CloseHandle(fileHandle);
			fileHandle = nullptr;
		}
	}
};

#endif