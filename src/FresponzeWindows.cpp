/*****************************************************************
* Copyright (C) Vertver, 2019. All rights reserved.
* Fresponze - fast, simple and modern multimedia sound library
* Apache-2 License
******************************************************************
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*****************************************************************/
#include "FresponzeTypes.h"
#define ALIGN_SIZE(Size, AlSize)        ((Size + (AlSize-1)) & (~(AlSize-1)))
#define ALIGN_SIZE_64K(Size)            ALIGN_SIZE(Size, 65536)
#define ALIGN_SIZE_16(Size)             ALIGN_SIZE(Size, 16)

HANDLE hFresponzeHeap = nullptr;
fr_u32 MemoryGranularity = 0;

bool
InitMemory()
{
	SYSTEM_INFO sysInfo = {};
	GetNativeSystemInfo(&sysInfo);
	MemoryGranularity = sysInfo.dwAllocationGranularity;
	hFresponzeHeap = HeapCreate(0, 0x010000, 0);
	return !IsInvalidHandle(hFresponzeHeap);
}

void
DestroyMemory()
{
	if (!IsInvalidHandle(hFresponzeHeap)) HeapDestroy(hFresponzeHeap);
}

void*
FastMemAlloc(
	fr_i32 SizeToAllocate
)
{
	return HeapAlloc(hFresponzeHeap, 0, SizeToAllocate);
}

void* 
VirtMemAlloc(
	fr_i64 SizeToAllocate
)
{
	return VirtualAlloc(nullptr, ALIGN_SIZE(SizeToAllocate, MemoryGranularity), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void
FreeFastMemory(
	void* Ptr
)
{
	HeapFree(hFresponzeHeap, 0, Ptr);
}

void 
FreeVirtMemory(
	void* Ptr, 
	size_t Size
)
{
	VirtualFree(Ptr, 0, MEM_RELEASE);
}

CWinEvent::CWinEvent()
{
	hEvent = CreateEventEx(nullptr, nullptr, 0, 0);
}

CWinEvent::~CWinEvent()
{
	if (!IsInvalidHandle(hEvent)) CloseHandle(hEvent);
}

void
CWinEvent::Raise()
{
	SetEvent(hEvent);
}

void
CWinEvent::Reset()
{
	ResetEvent(hEvent);
}

void
CWinEvent::Wait()
{
	WaitForSingleObject(hEvent, INFINITE);
}

bool
CWinEvent::Wait(
	fr_i32 TimeToWait
)
{
	return WaitForSingleObject(hEvent, (DWORD)TimeToWait) == WAIT_OBJECT_0;
}

bool
CWinEvent::IsRaised()
{
	return WaitForSingleObject(hEvent, 0) == WAIT_OBJECT_0;
}