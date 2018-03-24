// NativeLoader.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Api.h"

#ifdef DEBUG_CONSOLE

unsigned int CreateRequestIMP(const RString request)
{
	unsigned int req = 0;
	if (NLCreateRequest(5, request, &req) == EC_OK)
		return req;
	return 0;
}

int main()
{

	NLInitialize(5, true);
	NLAddHttpUrl(STR("http://rfpokerunity.cdnvideo.ru/ninx/WindowsPlayer/Live32/Content"));
	NLAddHttpUrl(STR("http://rfpkr.com/nginx/WindowsPlayer/Live32/Content"));
	NLAddFolder(STR("F:\\TestCppDownload"));

	CreateRequestIMP(STR("achievements_atlass.unity3d"));
	CreateRequestIMP(STR("animation_package_emotions_f.unity3d"));
	CreateRequestIMP(STR("animation_package_shop_anim_f.unity3d"));
	CreateRequestIMP(STR("gear_man_belt_v11.unity3d"));
	CreateRequestIMP(STR("gear_man_chain_v4.unity3d"));
	CreateRequestIMP(STR("gear_man_hat_v12.unity3d"));
	CreateRequestIMP(STR("gear_man_hat_v14.unity3d"));
	CreateRequestIMP(STR("gear_man_hat_v19.unity3d"));
	CreateRequestIMP(STR("room_skyscraper.unity3d"));
	CreateRequestIMP(STR("room_skyscraper_christmas.unity3d"));
	CreateRequestIMP(STR("room_skyscraper_halloween.unity3d"));
	CreateRequestIMP(STR("man_head_v2.unity3d"));

	uint32_t completed = 0;
	Symbol str[2048];
	while (completed < 12)
	{
		unsigned long long res = NLReleaseResult();
		if (res > 0)
		{
			uint32_t id = (uint32_t)((res & 0xFFFFFFFF00000000LL) >> 32);
			uint32_t status_code = (uint32_t)(res & 0xFFFFFFFFLL);
			completed++;
		}
		else
			std::this_thread::sleep_for(std::chrono::seconds(1));

		if (NLAcquireLogs(str, sizeof(str) / sizeof(Symbol) ))
		{
			wprintf(str);
		}
	}


	NLShutdown();

	return 0;
}

#endif