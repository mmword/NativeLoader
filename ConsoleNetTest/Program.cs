using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;

namespace ConsoleNetTest
{
    class Program
    {
        static void Main(string[] args)
        {

            if (PluginInterface.Initialize(5))
            {
                PluginInterface.AddHttpURL("http://test.rfpkr.com/nginx/Release/Content/");
                PluginInterface.AddFolder("G:\\TestCppDownload\\Hello");

                //uint iiid = PluginInterface.CreateRequest(3,0, "gear_man_hat_v3.unity3d");

                PluginInterface.CreateURLRequest(3, 0, "http://test.rfpkr.com/nginx/Release/Content/gamedata_client_test_RUS.xml");

                PluginInterface.CreateRequest(3,0, "animation_package_emotions_f.unity3d");
                PluginInterface.CreateRequest(3,0, "animation_package_shop_anim_f.unity3d");
                PluginInterface.CreateRequest(3,0, "gear_man_belt_v11.unity3d");
                PluginInterface.CreateRequest(3,0, "gear_man_chain_v4.unity3d");
                PluginInterface.CreateRequest(3,0, "gear_man_hat_v12.unity3d");
                PluginInterface.CreateRequest(3,0, "gear_man_hat_v14.unity3d");
                PluginInterface.CreateRequest(3,0, "gear_man_hat_v19.unity3d");
                PluginInterface.CreateRequest(3,0, "room_skyscraper.unity3d");
                PluginInterface.CreateRequest(3,0, "room_skyscraper_christmas.unity3d");
                PluginInterface.CreateRequest(3,0, "room_skyscraper_halloween.unity3d");
                PluginInterface.CreateRequest(3,0, "man_head_v2.unity3d");

                string lastError = string.Empty;
                uint res_id = 0;
                uint completed = 0;
                PluginInterface.OperationStatus status = PluginInterface.OperationStatus.OS_NotInitialized;

                while (completed < 12)
                {
                    while (!PluginInterface.DequeueResult(out res_id, out status, out lastError))
                        Task.Delay(1000).Wait();
                    ++completed;
                }

                Task.Delay(1000).Wait();
                PluginInterface.FreeThreadsIfPossible();

                PluginInterface.CreateURLRequest(3, 0, "http://test.rfpkr.com/nginx/Release/Content/gamedata_client_test_RUS.xml");

                Task.Delay(5000).Wait();

                PluginInterface.ShutDown();

            }

        }
    }
}
