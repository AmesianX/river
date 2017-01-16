#include "InprocessExecutionController.h"
#include "RiverStructs.h"
#include "../CommonCrossPlatform/Common.h"
#include "../revtracer-wrapper/RevtracerWrapper.h"
#include "../BinLoader/LoaderAPI.h"

#ifdef __linux__
#include <string.h>
#endif

#ifdef __linux__
#define CREATE_THREAD(tid, func, params, ret) do { ret = pthread_create((&tid), nullptr, (func), (params)); ret = (0 == ret); } while(false)
#define JOIN_THREAD(tid, ret) do { ret = pthread_join(tid, nullptr); ret = (0 == ret); } while (false)
#else
#define CREATE_THREAD(tid, func, params, ret) do { tid = CreateThread(nullptr, 0, (func), (params), 0, nullptr); ret = (tid != nullptr); } while (false)
#define JOIN_THREAD(tid, ret) do { ret = WaitForSingleObject(tid, INFINITE); ret = (WAIT_FAILED != ret); } while (false)
#endif

typedef ADDR_TYPE(*GetHandlerCallback)(void);

bool InprocessExecutionController::SetPath() {
	return false;
}

bool InprocessExecutionController::SetCmdLine() {
	return false;
}

THREAD_T InprocessExecutionController::GetProcessHandle() {
	return GET_CURRENT_PROC();
}

/*bool InprocessExecutionController::GetProcessVirtualMemory(VirtualMemorySection *&sections, int &sectionCount) {
	return false;
}

bool InprocessExecutionController::GetModules(ModuleInfo *& modules, int & moduleCount) {
	return false;
}*/

bool InprocessExecutionController::ReadProcessMemory(unsigned int base, unsigned int size, unsigned char * buff) {
	memcpy(buff, (LPCVOID)base, size);
	return true;
}

bool InprocessExecutionController::WriteProcessMemory(unsigned int base, unsigned int size, unsigned char * buff) {
	memcpy((LPVOID)base, buff, size);
	return true;
}

#ifdef __linux__
void *ThreadProc(void *p) {
	InprocessExecutionController *_this = (InprocessExecutionController *)p;
	_this->ControlThread();
  return nullptr;
}
#else
DWORD __stdcall ThreadProc(LPVOID p) {
	InprocessExecutionController *_this = (InprocessExecutionController *)p;
	return _this->ControlThread();
}
#endif

//unsigned int BranchHandlerFunc(void *context, void *userContext, void *nextInstruction);
void SyscallControlFunc(void *context, void *userContext);

typedef void(*InitializeFunc)();
typedef void(*ExecuteFunc)(int argc, char *argv[]);

InitializeFunc revtracerInitialize = nullptr;
ExecuteFunc revtraceExecute = nullptr;

bool InprocessExecutionController::Execute() {
	MODULE_PTR hRevTracerModule, hRevWrapperModule;
	BASE_PTR hRevTracerBase, hRevWrapperBase;
#ifdef _WIN32
	wchar_t revWrapperPath[] = L"revtracer-wrapper.dll";
#else
	wchar_t revWrapperPath[] = L"librevtracerwrapper.so";
#endif

	LOAD_LIBRARYW(L"revtracer.dll", hRevTracerModule, hRevTracerBase);
	LOAD_LIBRARYW(revWrapperPath, hRevWrapperModule, hRevWrapperBase);

	if (((0 == hRevTracerModule) && (0 == hRevTracerBase)) || 
            ((0 == hRevWrapperModule) && (0 == hRevWrapperBase))) {
		DEBUG_BREAK;
		return false;
	}

	rev::RevtracerAPI *api = (rev::RevtracerAPI *)GET_PROC_ADDRESS(hRevTracerModule, hRevTracerBase, "revtracerAPI");
	revCfg = (rev::RevtracerConfig *)GET_PROC_ADDRESS(hRevTracerModule, hRevTracerBase, "revtracerConfig");

	if (nullptr == api) {
		DEBUG_BREAK;
		return false;
	}

	api->dbgPrintFunc = ::DebugPrintf;

	api->branchHandler = BranchHandlerFunc;
	api->syscallControl = SyscallControlFunc;

	if (nullptr != trackCb) {
		api->trackCallback = trackCb;
	}

	if (nullptr != markCb) {
		api->markCallback = markCb;
	}

	if (nullptr != symbCb) {
		api->symbolicHandler = symbCb;
	}

	ADDR_TYPE initHandler = GET_PROC_ADDRESS(hRevWrapperModule, hRevWrapperBase, "InitRevtracerWrapper");
	if (!initHandler) {
		DEBUG_BREAK;
		return false;
	}

	if (nullptr == ((GetHandlerCallback)initHandler)()) {
		DEBUG_BREAK;
		return false;
	}

	api->lowLevel.ntAllocateVirtualMemory = GET_PROC_ADDRESS(hRevWrapperModule, hRevWrapperBase, "CallAllocateMemoryHandler");
	api->lowLevel.ntFreeVirtualMemory = GET_PROC_ADDRESS(hRevWrapperModule, hRevWrapperBase, "CallFreeMemoryHandler");
	api->lowLevel.ntTerminateProcess = GET_PROC_ADDRESS(hRevWrapperModule, hRevWrapperBase, "CallTerminateProcessHandler");
	api->lowLevel.vsnprintf_s = GET_PROC_ADDRESS(hRevWrapperModule, hRevWrapperBase, "CallFormattedPrintHandler");

	gcr = (GetCurrentRegistersFunc)GET_PROC_ADDRESS(hRevTracerModule, hRevTracerBase, "GetCurrentRegisters");
	gmi = (GetMemoryInfoFunc)GET_PROC_ADDRESS(hRevTracerModule, hRevTracerBase, "GetMemoryInfo");
	mmv = (MarkMemoryValueFunc)GET_PROC_ADDRESS(hRevTracerModule, hRevTracerBase, "MarkMemoryValue");
	glbbi = (GetLastBasicBlockInfoFunc)GET_PROC_ADDRESS(hRevTracerModule, hRevTracerBase, "GetLastBasicBlockInfo");

	if ((nullptr == gcr) || (nullptr == gmi) || (nullptr == mmv) || (nullptr == glbbi)) {
		DEBUG_BREAK;
		return false;
	}

	revCfg->entryPoint = entryPoint;
	revCfg->featureFlags = featureFlags;
	revCfg->context = this;
	
	revtracerInitialize = (InitializeFunc)GET_PROC_ADDRESS(hRevTracerModule, hRevTracerBase, "Initialize");
	revtraceExecute = (ExecuteFunc)GET_PROC_ADDRESS(hRevTracerModule, hRevTracerBase, "Execute");

	int ret;
	CREATE_THREAD(hThread, ThreadProc, this, ret);

#ifdef _WIN32
	InitSegments(hThread, revCfg->segmentOffsets);
#endif

	return TRUE == ret;
}


DWORD InprocessExecutionController::ControlThread() {
	revtracerInitialize();
	revtraceExecute(0, nullptr);

	execState = TERMINATED;
	observer->TerminationNotification(context);
	return 0;
}

bool InprocessExecutionController::WaitForTermination() {
	int ret;
	JOIN_THREAD(hThread, ret);
	return TRUE == ret;
}


/*void InprocessExecutionController::GetCurrentRegisters(Registers &registers) {
	RemoteRuntime *ree = (RemoteRuntime *)revCfg->pRuntime;

	memcpy(&registers, (Registers *)ree->registers, sizeof(registers));
	registers.esp = ree->virtualStack;
}*/

