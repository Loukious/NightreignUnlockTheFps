#include <Windows.h>
#include <algorithm>

#include "ModUtils.h"

using namespace ModUtils;
using namespace mINI;

static float fpsLimit = 300;

void ReadConfig()
{
	INIFile config(GetModFolderPath() + "\\config.ini");
	INIStructure ini;

	if (config.read(ini))
	{
		fpsLimit = std::stof(ini["unlockthefps"].get("limit"));
	}
	else
	{
		ini["unlockthefps"]["limit"] = "300";
		config.write(ini, true);
	}

	Log("FPS limit: ", fpsLimit);
}

DWORD WINAPI MainThread(LPVOID lpParam)
{
	Log("Activating UnlockTheFps...");
	{
		ReadConfig();

		const std::string PATTERN_UNLOCK_FPS = "c7 43 ? ? 88 88 3c";
		const int PATTERN_UNLOCK_FPS_OFFSET = 3;

		const std::string EXPECTED_UNLOCK_FPS_BYTES = "? 88 88 3c";
		std::string patchedUnlockFpsBytes = "90 90 90 90";

		float frametime = (1000.0f / fpsLimit) / 1000.0f;
		Log("Frametime: ", frametime);

		std::vector<unsigned char> frametimeBytes(sizeof(float), 0);
		MemCopy((uintptr_t)&frametimeBytes[0], (uintptr_t)&frametime, sizeof(float));
		patchedUnlockFpsBytes = RawAobToStringAob(frametimeBytes);

		uintptr_t patchAddress = AobScan(PATTERN_UNLOCK_FPS);
		if (patchAddress == 0)
		{
			Log("UnlockTheFps pattern not found!");
			return 1;
		}

		patchAddress += PATTERN_UNLOCK_FPS_OFFSET;

		if (!ReplaceExpectedBytesAtAddress(patchAddress, EXPECTED_UNLOCK_FPS_BYTES, patchedUnlockFpsBytes))
		{
			Log("Failed to patch UnlockTheFps at address: 0x%p", (void*)patchAddress);
			return 1;
		}
	}

	Log("Removing 60 FPS fullscreen limit...");
	{
		const std::string PATTERN_HERTZLOCK = "c7 45 ? 3c 00 00 00 c7 45 ? 01 00 00 00";
		const int PATTERN_HERTZLOCK_OFFSET_INTEGER1 = 3;
		const int PATTERN_HERTZLOCK_OFFSET_INTEGER2 = 10;

		const std::string EXPECTED_FPS_BYTES = "3c 00 00 00";
		const std::string PATCHED_FPS_BYTES = "00 00 00 00";

		const std::string EXPECTED_FLAG_BYTES = "01 00 00 00";
		const std::string PATCHED_FLAG_BYTES = "00 00 00 00";

		uintptr_t patchAddress = AobScan(PATTERN_HERTZLOCK);
		if (patchAddress == 0)
		{
			Log("Pattern not found!");
			return 1;
		}

		uintptr_t fpsValueAddr = patchAddress + PATTERN_HERTZLOCK_OFFSET_INTEGER1;
		uintptr_t flagAddr = patchAddress + PATTERN_HERTZLOCK_OFFSET_INTEGER2;

		if (!ReplaceExpectedBytesAtAddress(fpsValueAddr, EXPECTED_FPS_BYTES, PATCHED_FPS_BYTES))
		{
			Log("Failed to patch FPS limit at address: 0x%p", (void*)fpsValueAddr);
			return 1;
		}

		if (!ReplaceExpectedBytesAtAddress(flagAddr, EXPECTED_FLAG_BYTES, PATCHED_FLAG_BYTES))
		{
			Log("Failed to patch refresh rate flag at address: 0x%p", (void*)flagAddr);
			return 1;
		}
		Log("Successfully removed 60 FPS limit!");
	}

	CloseLog();
	return 0;
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(module);
		CreateThread(0, 0, &MainThread, 0, 0, NULL);
	}
	return 1;
}