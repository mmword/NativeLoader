#pragma once
#include "stdafx.h"
#include "Defines.h"
#include "LoaderInterface.h"
#include "PlatformBase.h"

#ifdef UNITY_WIN
#include "WindowsLoader.h"
#define FACTORY_LOADER WinInetWindowsLoader
#else
#error "cant find implementation iNativeFileLoader for current interface"
#endif

void WorkedThread(void *data);

struct ThreadDesc : iThread
{
	volatile bool isWorking;
	volatile bool keepLive;
	std::thread _thread;
	ThreadDesc() : isWorking(false), keepLive(true), _thread(WorkedThread,this) {}
	virtual const void Sleep(uint32_t milliseconds) const
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
	}
};

struct DownloadRequestComparer : std::binary_function<DownloadRequest*,DownloadRequest*, bool>
{
	bool operator()(const DownloadRequest* a, const DownloadRequest* b) const
	{
		if (a != nullptr && b != nullptr)
			return a->GetPriority() < b->GetPriority();
		return false;
	}
};

typedef std::unordered_map<uint32_t, DownloadRequest*> req_map;
typedef std::unordered_map<uint32_t, std::unique_ptr<DownloadRequest>> req_map_safe;
typedef std::queue<DownloadRequest*> req_queue;
typedef std::priority_queue<DownloadRequest*, std::vector<DownloadRequest*>, DownloadRequestComparer> preq_queue;
typedef std::vector<DownloadRequest*> req_vec;
typedef std::lock_guard<std::mutex> lock_guard;
typedef std::unique_lock<std::mutex> unique_lock;

static std::vector<String> hosts;
static std::vector<String> folders_destination;

static preq_queue requests;
static req_queue results;
static req_map processed_requests;
static req_map_safe all_requests_pointers;
static std::vector<ThreadDesc> threadsDescriptions;
static volatile std::atomic_int numResults = 0,numRequests = 0;

static std::mutex gCollectionMutex;
static std::condition_variable queue_event;
static bool gInit = false;
static int gNumThreads = 0;

static uint32_t gID = 1;

template<typename T>
void ClearQueue(T &q)
{
	if (!q.empty())
	{
		T empty;
		std::swap(q, empty);
	}
}

void ClearData()
{
	hosts.clear();
	folders_destination.clear();
	ClearQueue(requests);
	ClearQueue(results);
	processed_requests.clear();
	all_requests_pointers.clear();
	threadsDescriptions.clear();
	numResults = numRequests = 0;
	gInit = false;
	gID = 1;
	gNumThreads = 0;
}

bool ManageRequestError(DownloadRequest *req)
{
	if (req != nullptr)
	{
		if (!req->last_error.empty())
		{
			if (!req->IncreaseAttempt())
			{
				if (!((req->atomicFlags | R_FullUrlPath) > 0) && !req->MoveToNextHostAndResetAttempts())
				{
					return false;
				}
				else
					req->last_error.clear();
			}
			else
				req->last_error.clear();
		}
	}
	return true;
}

void WorkedThread(void *data)
{
	ThreadDesc *desc = (ThreadDesc*)data;
	while (desc->keepLive)
	{
		desc->isWorking = false;
		DownloadRequest *req = nullptr;
		{
			unique_lock gcLock(gCollectionMutex);
			if(requests.empty())
				queue_event.wait(gcLock);
			if (!requests.empty()) // if another thread catch queue or queue clear after wait , skip front invoking
			{
				req = requests.top();
				requests.pop();
				numRequests--;
			}
			gcLock.unlock();
		}
		if (req != nullptr)
		{
			desc->isWorking = true;
			req->status = OS_Processed;
			FACTORY_LOADER fileLoader;
			{
				lock_guard gcLock(gCollectionMutex);
				processed_requests.insert(std::make_pair(req->id, req));
				fileLoader.ThreadSafeConstruction(req, desc);
			}
			bool mCreate = fileLoader.Create();
			if (!mCreate && req->manageAttempts)
			{
				while (ManageRequestError(req) && !mCreate)
				{
					fileLoader.Release();
					mCreate = fileLoader.Create();
				}
			}
			if (mCreate)
			{
				int iterations = 0;
				while (fileLoader.Process())
				{
					if (req->status == OS_Abort || req->status == OS_Terminate)
					{
						fileLoader.Abort();
						break;
					}
					if (!req->last_error.empty() && req->manageAttempts)
					{
						if (!ManageRequestError(req))
						{
							req->status = OS_Fail;
							break;
						}
						fileLoader.Release();
						fileLoader.Create();
					}
					if (iterations++ > 256)
					{
						iterations = 0;
						if (numRequests > 0)
							std::this_thread::yield();
					}
				}
			}
			else
				req->status = OS_Fail;

			req->atomicFlags |= R_Finished;

			if (req->status == OS_Processed)
				req->status = OS_Finished;

			if (req->status != OS_Terminate)
			{
				lock_guard lc(gCollectionMutex);
				results.push(req);
				processed_requests.erase(req->id);
				numResults++;
			}
			else
			{
				lock_guard lc(gCollectionMutex);
				processed_requests.erase(req->id);
			}

		}
	}
}

void AbortDownload(uint32_t id)
{
	if (gInit)
	{
		lock_guard lc(gCollectionMutex);
		req_map::const_iterator it = processed_requests.find(id);
		if (it != processed_requests.cend())
		{
			it->second->ChangeToAbort();
		}
	}
}

void TerminateAllDownloads(bool cleanCollections)
{
	if (gInit)
	{
		lock_guard lc(gCollectionMutex);
		for (auto it = processed_requests.cbegin(); it != processed_requests.cend(); ++it)
		{
			if (it->second->GetStatus() == OS_Processed)
				it->second->ChangeToTerminate();
		}
		if (cleanCollections)
		{
			processed_requests.clear();
			ClearQueue(results);
			ClearQueue(requests);
			numResults = 0;
		}
	}
}

ErrorCode EnqueueRequest(std::unique_ptr<DownloadRequest> &&req)
{
	bool res = false;
	if (gInit)
	{
		ErrorCode code = EC_OK;
		unique_lock gcLock(gCollectionMutex);
		res = processed_requests.find(req->GetID()) == processed_requests.cend();
		if (res)
		{
			requests.push(req.get());
			all_requests_pointers.insert_or_assign(req->GetID(), std::move(req));
			numRequests++;
			queue_event.notify_one();
		}
		else
			code = EC_AlreadyExist;
		gcLock.unlock();
		return code;
	}
	return EC_NotInitialized;
}

DownloadRequest *DequeueResult()
{
	DownloadRequest *result = nullptr;
	if (gInit)
	{
		lock_guard lc(gCollectionMutex);
		if (!results.empty())
		{
			result = results.front();
			results.pop();
			numResults--;
		}
	}
	return result;
}

void DequeueAndFreeResult(std::function<void(DownloadRequest*)> map)
{
	if (map != nullptr && gInit)
	{
		lock_guard lc(gCollectionMutex);
		if (!results.empty())
		{
			auto result = results.front();
			results.pop();
			numResults--;

			map(result);

			auto it = all_requests_pointers.find(result->GetID());
			if (it != all_requests_pointers.cend())
			{
				it->second.reset();
				all_requests_pointers.erase(it);
			}
		}
	}
}

void WakeUpThreads()
{
	if(gInit && threadsDescriptions.empty())
		threadsDescriptions.resize(gNumThreads);
}

bool Initialize(int numThreads)
{
	if (numThreads < 1 || numThreads > 15)
	{
		numThreads = std::thread::hardware_concurrency();
	}
	if (!gInit)
	{
		gNumThreads = numThreads;
		gInit = true;
		WakeUpThreads();
	}
	return gInit;
}

void Shutdown()
{
	if (gInit)
	{
		TerminateAllDownloads(true);
		for (auto it = threadsDescriptions.begin(); it != threadsDescriptions.end(); ++it)
		{
			it->keepLive = false;
		}
		{
			unique_lock gcLock(gCollectionMutex);
			queue_event.notify_all();
			gcLock.unlock();
		}
		for (auto it = threadsDescriptions.begin(); it != threadsDescriptions.end(); ++it)
		{
			auto thr = &it->_thread;
			if (thr->joinable())
				thr->join();
		}
		ClearData();
	}
}

void AddHttpUrl(const RString url)
{
	lock_guard gcLock(gCollectionMutex);
	String _url(url);
	hosts.push_back(_url);
}

void AddFolder(const RString folder)
{
	lock_guard gcLock(gCollectionMutex);
	String _path(folder);
	folders_destination.push_back(_path);
}

bool AllocDownloadRequest(uint32_t max_attempts, uint32_t priority, const RString filename,std::unique_ptr<DownloadRequest> &ptr)
{
	if (gInit)
	{
		DownloadRequest *req = new DownloadRequest(&folders_destination, &hosts, gID++, max_attempts, filename, priority);
		ptr.reset(req);
		return true;
	}
	return false;
}

ErrorCode CreateRequest(uint32_t max_attempts, uint32_t priority, const RString filename, DownloadRequest **dest)
{
	if (gInit)
	{
		WakeUpThreads();
		std::unique_ptr<DownloadRequest> _ptr;
		if (hosts.empty() || folders_destination.empty() || filename == nullptr || !AllocDownloadRequest(max_attempts, priority, filename, _ptr))
			return EC_WrongParameter;
		*dest = _ptr.get();
		return EnqueueRequest(std::move(_ptr));
	}
	return EC_NotInitialized;
}

ErrorCode CreateUrlRequest(uint32_t max_attempts, uint32_t priority, const RString url, DownloadRequest **dest)
{
	if (gInit)
	{
		WakeUpThreads();
		std::unique_ptr<DownloadRequest> _ptr;
		if (folders_destination.empty() || url == nullptr || !AllocDownloadRequest(max_attempts, priority, url, _ptr))
			return EC_WrongParameter;
		*dest = _ptr.get();
		(*dest)->SetAtomicFlags((RequestAtomicFlags)((*dest)->GetAtomicFlags() | R_FullUrlPath));
		return EnqueueRequest(std::move(_ptr));
	}
	return EC_NotInitialized;
}

ErrorCode SetRequestPriority(uint32_t id,uint32_t priority)
{
	if (gInit)
	{
		{
			lock_guard gcLock(gCollectionMutex);
			auto it = all_requests_pointers.find(id);
			if (it != all_requests_pointers.cend())
			{
				if (it->second->GetStatus() == OS_PrepareToDownload)
				{
					it->second->SetPriority(priority);
					auto ptr = const_cast<DownloadRequest**>(&requests.top());
					std::make_heap(ptr, ptr + requests.size(), DownloadRequestComparer());
					return EC_OK;
				}
			}
			return EC_NotExist;
		}
	}
	return EC_NotInitialized;
}

ErrorCode ReleaseRequestByID(uint32_t id)
{
	if (gInit)
	{
		auto it = all_requests_pointers.find(id);
		if (it != all_requests_pointers.cend())
		{
			if (it->second->GetStatus() == OS_Processed)
				return EC_AlreadyProcessed;
			it->second.reset(nullptr);
			all_requests_pointers.erase(it);
			return EC_OK;
		}
		else
			return EC_NotExist;
	}
	return EC_NotInitialized;
}

ErrorCode ReleaseRequest(DownloadRequest *req)
{
	if (req == nullptr)
		return EC_WrongParameter;
	return ReleaseRequestByID(req->GetID());
}

ErrorCode ReleaseAllRequests()
{
	if (gInit)
	{
		std::lock_guard<std::mutex> gcLock(gCollectionMutex);
		all_requests_pointers.clear(); // all smatrpointer should destructed
		return EC_OK;
	}
	return EC_NotInitialized;
}

void FreeThreadsIfPossible()
{
	if (!threadsDescriptions.empty())
	{
		int nonWorks = 0;
		for (auto it = threadsDescriptions.cbegin(); it != threadsDescriptions.cend(); ++it)
		{
			if (!it->isWorking)
				nonWorks++;
		}
		if (nonWorks == (threadsDescriptions.size()))
		{
			for (auto it = threadsDescriptions.begin(); it != threadsDescriptions.end(); ++it)
			{
				it->keepLive = false;
			}
			unique_lock gcLock(gCollectionMutex);
			queue_event.notify_all();
			gcLock.unlock();
			for (auto it = threadsDescriptions.begin(); it != threadsDescriptions.end(); ++it)
			{
				auto thr = &it->_thread;
				if (thr->joinable())
					thr->join();
			}
			threadsDescriptions.clear();
		}
	}

}

bool HasResults()
{
	return numResults > 0;
}