using System.Text;
using System.Runtime.InteropServices;

namespace ConsoleNetTest
{
    public static class PluginInterface
    {
        const string module = "NativeLoader.dll";
        static StringBuilder lastError = new StringBuilder(2048);

        [DllImport(module)]
        static extern int NLInitialize(int pnumThreads);
        [DllImport(module)]
        static extern void NLShutdown();
        [DllImport(module)]
        static extern void NLAddHttpUrl([MarshalAs(UnmanagedType.LPWStr)]string purl);
        [DllImport(module)]
        static extern void NLAddFolder([MarshalAs(UnmanagedType.LPWStr)]string pfolder);
        [DllImport(module)]
        static extern int NLCreateRequest(uint pmaxAttempts,uint priority, [MarshalAs(UnmanagedType.LPWStr)]string pfilename,ref uint presID);
        [DllImport(module)]
        static extern int NLCreateUrlRequest(uint pmaxAttempts, uint priority, [MarshalAs(UnmanagedType.LPWStr)]string pfilename, ref uint presID);
        [DllImport(module)]
        static extern int NLReleaseRequest(uint id);
        [DllImport(module)]
        static extern int NLReleaseAllRequests();
        [DllImport(module)]
        static extern ulong NLReleaseResult();
        [DllImport(module,CharSet=CharSet.Unicode)]
        static extern int NLDequeueResult(ref uint id, ref uint status,StringBuilder last_error,uint buffSize);
        [DllImport(module)]
        static extern void NLAbortDownload(uint id);
        [DllImport(module)]
        static extern int NLSetRequestPriority(uint pid, uint priority);
        [DllImport(module)]
        static extern void NLFreeThreadsIfPossible();

        public enum OperationStatus
        {
            OS_NotInitialized,
            OS_PrepareToDownload,
            OS_Processed,
            OS_Abort,
            OS_Terminate,
            OS_Fail,
            OS_Finished
        }

        public static bool Initialize(int numThreads)
        {
            return NLInitialize(numThreads) > 0;
        }

        public static void ShutDown()
        {
            NLShutdown();
        }

        public static void AddHttpURL(string url)
        {
            NLAddHttpUrl(url);
        }

        public static void AddFolder(string folder)
        {
            NLAddFolder(folder);
        }

        public static uint CreateRequest(uint pmaxAttempts,uint priority, string pfilename)
        {
            uint id = 0;
            NLCreateRequest(pmaxAttempts, priority, pfilename, ref id);
            return id;
        }

        public static uint CreateURLRequest(uint pmaxAttempts, uint priority, string url)
        {
            uint id = 0;
            NLCreateUrlRequest(pmaxAttempts, priority, url, ref id);
            return id;
        }

        public static void ReleaseRequest(uint id)
        {
            NLReleaseRequest(id);
        }

        public static bool ReleaseResult(out uint id,out OperationStatus status_code)
        {
            id = 0;
            status_code = OperationStatus.OS_NotInitialized;
            ulong res = NLReleaseResult();
            if (res > 0)
            {
                id = (uint)((res & 0xFFFFFFFF00000000L) >> 32);
                status_code = (OperationStatus)(uint)(res & 0xFFFFFFFFL);
                return true;
            }
            return false;
        }

        public static bool DequeueResult(out uint id, out OperationStatus status_code, out string last_error)
        {
            id = 0;
            uint code = 0;
            status_code = OperationStatus.OS_NotInitialized;
            last_error = string.Empty;
            if(NLDequeueResult(ref id,ref code,lastError,2048) > 0)
            {
                status_code = (OperationStatus)code;
                last_error = lastError.ToString();
                return true;
            }
            return false;
        }

        public static void AbortRequest(uint id)
        {
            NLAbortDownload(id);
        }

        public static void FreeThreadsIfPossible()
        {
            NLFreeThreadsIfPossible();
        }

        public static OperationStatus SetPriority(uint id,uint priority)
        {
            return (OperationStatus)NLSetRequestPriority(id, priority);
        }
    }

}
