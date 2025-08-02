/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	threads - manage threads on the starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <types.h>
#include <string.h>
#include <ios/processor.h>
#include <ios/gecko.h>
#include <ios/errno.h>
#include <ios/module.h>

#include "core/defines.h"
#include "core/iosElf.h"
#include "interrupt/irq.h"
#include "scheduler/threads.h"
#include "messaging/ipc.h"
#include "filedesc/calls.h"
#include "memory/memory.h"
#include "memory/heaps.h"

#include "panic.h"

extern const u32 __thread_stacks_area_start[];
extern const u32 __thread_stacks_area_size[];

#ifndef MIOS
extern u32 *MemoryTranslationTable;
extern u32 DomainAccessControlTable[MAX_PROCESSES];
extern u32 *HardwareRegistersAccessTable[MAX_PROCESSES];
#endif

void EndThread();
#define STACK_SIZE 0x400

u32 ProcessUID[MAX_PROCESSES] = { 0 };
u16 ProcessGID[MAX_PROCESSES] = { 0 };
ThreadInfo Threads[MAX_THREADS] SRAM_DATA ALIGNED(0x10) = { 0 };
ThreadQueue SchedulerQueue ALIGNED(0x04) = { .NextThread = &ThreadStartingState };
ThreadInfo ThreadStartingState ALIGNED(0x04) = { .Priority = -1 };
ThreadInfo *CurrentThread ALIGNED(0x10) = NULL;
void *ThreadEndFunction = NULL;

static inline s32 _GetThreadID(ThreadInfo *thread)
{
	u32 offset = (u32)thread - (u32)(&Threads[0]);
	return offset == 0 ? 0 : (s32)(offset / sizeof(ThreadInfo));
}

void InitializeThreadContext()
{
#ifndef MIOS
 //copy function to mem2 where everything can access it
	ThreadEndFunction = KMalloc(0x10);
	memcpy(ThreadEndFunction, EndThread, 0x10);
#endif

 //Initilize thread structures & set stack pointers
	for (u16 i = 0; i < MAX_PROCESSES; i++)
	{
		ProcessUID[i] = i;
		ProcessGID[i] = i;
	}

#ifndef MIOS

	memset((void *)__thread_stacks_area_start, 0xA5, (u32)__thread_stacks_area_size);

	for (u16 i = 0; i < MAX_THREADS; i++)
	{
		//gcc works by having a downwards stack, hence setting the stack to the upper limit
		Threads[i].DefaultThreadStack =
		    ((u32)&__thread_stacks_area_start) + (u32)(STACK_SIZE * (i + 1));
	}

#endif
}

//Scheduler
void ThreadQueue_RemoveThread(ThreadQueue *threadQueue, ThreadInfo *threadToRemove)
{
	if (threadQueue == NULL || threadToRemove == NULL)
		return;

	ThreadInfo *thread = threadQueue->NextThread;
	while (thread)
	{
		if (thread == threadToRemove)
		{
			threadQueue->NextThread = threadToRemove->NextThread;
			break;
		}

		threadQueue = (ThreadQueue *)thread->NextThread;
		thread = thread->NextThread;
	}

	return;
}

void ThreadQueue_PushThread(ThreadQueue *threadQueue, ThreadInfo *thread)
{
	//not sure if this is correct. it works, and seems to be what the asm in ios kinda looks like
	//however, looking in ghidra it looks completely different. what is ghidra thinking, and why?
	if (threadQueue == NULL || thread == NULL)
		return;

	ThreadInfo *nextThread = threadQueue->NextThread;
	s32 threadPriority = thread->Priority;
	s32 nextPriority = nextThread->Priority;
	ThreadInfo **previousThread = &threadQueue->NextThread;

	while (threadPriority < nextPriority)
	{
		previousThread = &nextThread->NextThread;
		nextThread = nextThread->NextThread;
		nextPriority = nextThread->Priority;
	}

	*previousThread = thread;
	thread->ThreadQueue = threadQueue;
	thread->NextThread = nextThread;
	return;
}

ThreadInfo *ThreadQueue_PopThread(ThreadQueue *queue)
{
	ThreadInfo *ret = queue->NextThread;
	queue->NextThread = ret->NextThread;

	return ret;
}

__attribute__((target("arm"))) __attribute__((noreturn)) void ScheduleYield(void)
{
	CurrentThread = ThreadQueue_PopThread(&SchedulerQueue);
	CurrentThread->ThreadState = Running;

#ifndef MIOS
	SetDomainAccessControlRegister(DomainAccessControlTable[CurrentThread->ProcessId]);
	MemoryTranslationTable[0xD0] =
	    (u32)HardwareRegistersAccessTable[CurrentThread->ProcessId];
	TlbInvalidate();
	FlushMemory();
#endif

	__asm__ volatile(
	    "\
#ios loads the threads' state buffer back in to sp, resetting the exception's stack\n\
		msr		cpsr_c, #0xd3\n\
		add		sp, %[threadContext], %[stackOffset]\n\
		msr		cpsr_c, #0xdb\n\
		add		sp, %[threadContext], %[stackOffset]\n\
#store pointer of the context in lr\n\
		mov		lr, %[threadContext]\n\
#restore the status register\n\
		ldr		%[threadContext], [lr, #0x00]\n\
		msr		spsr_cxsf, %[threadContext]\n\
#restore the rest of the state\n\
		ldmib	lr, {r0-r12, sp, lr}^\n\
		ldr		lr, [lr, #0x40]\n\
#jump to thread\n\
		movs 	pc, lr\n"
	    :
	    : [threadContext] "r"(CurrentThread), [threadContextOffset] "J"(offsetof(ThreadInfo, ThreadContext)),
	      [stackOffset] "J"(offsetof(ThreadInfo, UserContext)));
	__builtin_unreachable();
}

//Called syscalls.
void YieldThread(void)
{
	u32 state = DisableInterrupts();
	CurrentThread->ThreadState = Ready;

	YieldCurrentThread(&SchedulerQueue);

	RestoreInterrupts(state);
}

void UnblockThread(ThreadQueue *threadQueue, s32 returnValue)
{
	ThreadInfo *nextThread = ThreadQueue_PopThread(threadQueue);
	nextThread->ThreadContext.Registers[0] = (u32)returnValue;
	nextThread->ThreadState = Ready;

	ThreadQueue_PushThread(&SchedulerQueue, nextThread);
	nextThread = CurrentThread;

	if (nextThread->Priority < SchedulerQueue.NextThread->Priority)
	{
		CurrentThread->ThreadState = Ready;
		YieldCurrentThread(&SchedulerQueue);
	}
}

//IOS Handlers
s32 CreateThread(u32 main, void *arg, u32 *stack_top, u32 stacksize, s32 priority, u32 detached)
{
	int threadId = 0;
	u32 irqState = DisableInterrupts();

#ifdef MIOS
	if ((u32)priority >= 0x80)
	{
		threadId = IPC_EINVAL;
		goto restore_and_return;
	}
#else
	if ((u32)priority >= 0x80 || (stack_top != NULL && stacksize == 0) ||
	    (CurrentThread != NULL && priority > CurrentThread->InitialPriority))
	{
		threadId = IPC_EINVAL;
		goto restore_and_return;
	}
#endif

	ThreadInfo *selectedThread;
	while (threadId < MAX_THREADS)
	{
		selectedThread = &Threads[threadId];
		if (selectedThread->ThreadState == Unset)
			break;

		threadId++;
	}

	if (threadId >= MAX_THREADS)
	{
		threadId = IPC_EMAX;
		goto restore_and_return;
	}

	selectedThread->ProcessId = (CurrentThread == NULL) ? 0 : CurrentThread->ProcessId;
	selectedThread->ThreadState = Stopped;
	selectedThread->Priority = priority;
	selectedThread->ThreadContext.ProgramCounter = main;
	selectedThread->ThreadContext.Registers[0] = (u32)arg;

#ifdef MIOS
	selectedThread->ThreadContext.LinkRegister = (u32)EndThread;
	selectedThread->ThreadContext.StackPointer = (u32)stack_top;
#else
	selectedThread->InitialPriority = priority;
	selectedThread->ThreadContext.LinkRegister = (u32)ThreadEndFunction;
	selectedThread->ThreadContext.StackPointer =
	    stack_top == NULL ? selectedThread->DefaultThreadStack : (u32)stack_top;
#endif

	//set thread state correctly
	selectedThread->ThreadContext.StatusRegister =
	    ((main & 0x01) == 1) ? (SPSR_USER_MODE | SPSR_THUMB_MODE) : SPSR_USER_MODE;
	selectedThread->NextThread = NULL;
	selectedThread->ThreadQueue = NULL;
	selectedThread->JoinQueue = NULL;
	selectedThread->IsDetached = detached;

restore_and_return:
	RestoreInterrupts(irqState);
	return threadId;
}

s32 StartThread(const s32 threadId)
{
	u32 irqState = DisableInterrupts();
	s32 ret = 0;
	ThreadQueue *threadQueue = NULL;

	if (threadId < 0 || threadId >= MAX_THREADS)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	ThreadInfo *threadToStart =
	    (threadId == 0 && CurrentThread != NULL) ? CurrentThread : &Threads[threadId];

	//does the current thread even own the thread?
	if (CurrentThread != NULL && CurrentThread->ProcessId != 0 &&
	    threadToStart->ProcessId != CurrentThread->ProcessId)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	if (threadToStart->ThreadState != Stopped)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	threadQueue = threadToStart->ThreadQueue;
	if (threadQueue == NULL || threadQueue == &SchedulerQueue)
	{
		threadToStart->ThreadState = Ready;
		ThreadQueue_PushThread(&SchedulerQueue, threadToStart);
	}
	else
	{
		threadToStart->ThreadState = Waiting;
		ThreadQueue_PushThread(threadQueue, threadToStart);
	}

	if (CurrentThread == NULL)
		ScheduleYield();
	else if (CurrentThread->Priority < SchedulerQueue.NextThread->Priority)
	{
		CurrentThread->ThreadState = Ready;
		YieldCurrentThread(&SchedulerQueue);
	}

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 CancelThread(const s32 threadId, u32 return_value)
{
	u32 irqState = DisableInterrupts();
	s32 ret = 0;

	if (threadId < 0 || threadId > MAX_THREADS)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	ThreadInfo *threadToCancel =
	    (threadId == 0 && CurrentThread != NULL) ? CurrentThread : &Threads[threadId];

	//does the current thread even own the thread?
	if (CurrentThread != NULL && CurrentThread->ProcessId != 0 &&
	    threadToCancel->ProcessId != CurrentThread->ProcessId)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	threadToCancel->ReturnValue = return_value;
	if (threadToCancel->ThreadState != Stopped)
		ThreadQueue_RemoveThread(&SchedulerQueue, threadToCancel);

	if (!threadToCancel->IsDetached)
		threadToCancel->ThreadState = Dead;
	else
		threadToCancel->ThreadState = Unset;

	CurrentThread->ThreadContext.Registers[0] = (u32)ret;
	if (threadToCancel == CurrentThread)
		ScheduleYield();
	else if (CurrentThread->Priority < SchedulerQueue.NextThread->Priority)
	{
		CurrentThread->ThreadState = Ready;
		YieldCurrentThread(&SchedulerQueue);
	}

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 JoinThread(const s32 threadId, u32 *returnedValue)
{
	u32 irqState = DisableInterrupts();
	s32 ret = 0;

	if (threadId < 0 || threadId >= MAX_THREADS)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	ThreadInfo *threadToJoin =
	    (threadId == 0 && CurrentThread != NULL) ? CurrentThread : &Threads[threadId];

	//does the current thread even own the thread?
	if (CurrentThread != NULL && CurrentThread->ProcessId != 0 &&
	    threadToJoin->ProcessId != CurrentThread->ProcessId)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	if (threadToJoin == CurrentThread || threadToJoin->IsDetached)
		goto restore_and_return;

	ThreadState threadState = threadToJoin->ThreadState;
	if (threadState != Dead)
	{
		CurrentThread->ThreadState = Waiting;
		YieldCurrentThread(&SchedulerQueue);
		threadState = threadToJoin->ThreadState;
	}

	if (returnedValue != NULL)
		*returnedValue = threadToJoin->ReturnValue;

	if (threadState != Dead)
		gecko_printf("thread %d is not dead, but join from %d resumed\n",
		             _GetThreadID(threadToJoin), _GetThreadID(CurrentThread));

	threadToJoin->ThreadState = Unset;
restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 SuspendThread(const s32 threadId)
{
	u32 irqState = DisableInterrupts();
	s32 ret = 0;

	if (threadId < 0 || threadId >= MAX_THREADS)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	ThreadInfo *threadToSuspend =
	    (threadId == 0 && CurrentThread != NULL) ? CurrentThread : &Threads[threadId];

	//does the current thread even own the thread?
	if (CurrentThread != NULL && CurrentThread->ProcessId != 0 &&
	    threadToSuspend->ProcessId != CurrentThread->ProcessId)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	switch (threadToSuspend->ThreadState)
	{
		case Running:
			threadToSuspend->ThreadState = Stopped;
			YieldCurrentThread(NULL);
			break;
		case Ready:
		case Waiting:
			threadToSuspend->ThreadState = Stopped;
			ThreadQueue_RemoveThread(threadToSuspend->ThreadQueue, threadToSuspend);
			break;
		case Unset:
			break;
		default:
			ret = IPC_EINVAL;
			break;
	}

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 GetThreadID()
{
	return _GetThreadID(CurrentThread);
}

u32 GetProcessID()
{
	return CurrentThread->ProcessId;
}

s32 GetThreadPriority(const s32 threadId)
{
	u32 irqState = DisableInterrupts();

	ThreadInfo *thread;
	s32 ret;

	if (threadId == 0 && CurrentThread != NULL)
	{
		ret = CurrentThread->Priority;
		goto restore_and_return;
	}

	if (threadId < 0 || threadId >= MAX_THREADS)
		goto return_error;

	thread = &Threads[threadId];
	//does the current thread even own the thread?
	if (CurrentThread != NULL && CurrentThread->ProcessId != 0 &&
	    thread->ProcessId != CurrentThread->ProcessId)
		goto return_error;

	ret = thread->Priority;
	goto restore_and_return;

return_error:
	ret = IPC_EINVAL;
restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 SetThreadPriority(const s32 threadId, s32 priority)
{
	u32 irqState = DisableInterrupts();

	ThreadInfo *thread = NULL;
	s32 ret = 0;

	if (threadId < 0 || threadId > MAX_THREADS || (u32)priority >= 0x80)
		goto return_error;

	if (threadId == 0)
		thread = CurrentThread;

	if (thread == NULL)
		thread = &Threads[threadId];

	//does the current thread even own the thread?
	if (CurrentThread != NULL && CurrentThread->ProcessId != 0 &&
	    thread->ProcessId != CurrentThread->ProcessId)
		goto return_error;

#ifndef MIOS
	if (priority >= thread->InitialPriority)
		goto return_error;
#endif

	if (thread->Priority == priority)
		goto restore_and_return;

	thread->Priority = priority;
	if (thread != CurrentThread && thread->ThreadState != Stopped)
	{
		ThreadQueue_RemoveThread(&SchedulerQueue, thread);
		ThreadQueue_PushThread(&SchedulerQueue, thread);
	}

	if (CurrentThread->Priority < SchedulerQueue.NextThread->Priority)
	{
		CurrentThread->ThreadState = Ready;
		YieldCurrentThread(&SchedulerQueue);
	}
	goto restore_and_return;

return_error:
	ret = IPC_EINVAL;
restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

u32 GetUID(void)
{
	return ProcessUID[CurrentThread->ProcessId];
}

s32 SetUID(u32 pid, u32 uid)
{
	s32 ret = IPC_SUCCESS;
	u32 irqState = DisableInterrupts();

	if (pid >= MAX_PROCESSES)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	if (CurrentThread->ProcessId >= 2)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}

	ProcessUID[pid] = uid;

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

u16 GetGID(void)
{
	return ProcessGID[CurrentThread->ProcessId];
}

s32 SetGID(u32 pid, u16 gid)
{
	s32 ret = IPC_SUCCESS;
	u32 irqState = DisableInterrupts();

	if (pid >= MAX_PROCESSES)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	if (CurrentThread->ProcessId >= 2)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}

	ProcessGID[pid] = gid;

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}
#ifndef MIOS
s32 LaunchRM(const char *path)
{
	if (GetUID() != 0)
		return IPC_EACCES;

	//*technically* heapid 0 isn't correct here. the kernel heap id just happens to be always 0, but... :)
	//but hey, this is what IOS did!
	Elf32_Ehdr *elfHeader = (Elf32_Ehdr *)AllocateOnHeap(0, sizeof(Elf32_Ehdr));
	if (elfHeader == NULL)
		return IPC_EMAX;

	const u32 elfMagic = ELFMAGIC;
	Elf32_Nhdr *noteHeader = NULL;
	Elf32_Phdr *programHeaders = NULL;
	s32 fd = OpenFD(path, IOS_OPEN);
	s32 ret = fd;
	if (ret < 0)
		goto cleanup_launch;

	if (ReadFD(fd, elfHeader, 4) != 4)
		goto cleanup_launch;

	if (memcmp(elfHeader, &elfMagic, 4) != 0)
	{
		ret = 0;
		goto cleanup_launch;
	}

	ret = SeekFD(fd, 0, 0);
	if (ret != 0)
		goto cleanup_launch;

	ret = ReadFD(fd, elfHeader, sizeof(Elf32_Ehdr));
	if (ret != sizeof(Elf32_Ehdr))
		goto cleanup_launch;

	if (ELFMAGIC != *((u32 *)&elfHeader->e_ident[EI_MAG0]) ||
	    IOSELFINFO != *((u32 *)&elfHeader->e_ident[EI_CLASS]))
	{
		ret = IPC_EINVAL;
		goto cleanup_launch;
	}

	if (elfHeader->e_machine != EM_ARM || elfHeader->e_type != ET_EXEC ||
	    elfHeader->e_version != 1 || (elfHeader->e_flags & 0x21) != 0)
	{
		ret = IPC_EINVAL;
		goto cleanup_launch;
	}

	programHeaders =
	    (Elf32_Phdr *)AllocateOnHeap(0, sizeof(Elf32_Phdr) * elfHeader->e_phnum);
	if (!programHeaders)
	{
		ret = IPC_EMAX;
		goto cleanup_launch;
	}

	ret = ReadFD(fd, programHeaders, sizeof(Elf32_Phdr) * elfHeader->e_phnum);
	if (ret != (s32)(sizeof(Elf32_Phdr) * elfHeader->e_phnum))
		goto cleanup_launch;

	noteHeader = AllocateOnHeap(0, 0x4000);
	if (!noteHeader)
	{
		ret = IPC_EMAX;
		goto cleanup_launch;
	}

	u32 headerCount = elfHeader->e_phnum;
	u32 noteLength = 0;
	u32 noteOffset = 0;
	for (u32 headerIndex = 0; headerIndex < headerCount; headerIndex++)
	{
		Elf32_Phdr *programHeader = programHeaders + headerIndex;
		if (programHeader->p_type == PT_LOAD)
		{
			noteLength = programHeader->p_filesz;
			noteOffset = programHeader->p_offset;
			continue;
		}

		if (programHeader->p_type != PT_LOAD || programHeader->p_vaddr == 0)
			continue;

		MemorySection section = { .PhysicalAddress = programHeader->p_paddr,
			                      .VirtualAddress = programHeader->p_vaddr,
			                      .Size = (programHeader->p_memsz + 0xFFF) & 0xFFFFF000,
			                      .Domain = 0,
			                      .AccessRights = AP_RWUSER,
			                      .IsCached = 1 };

		if (MapMemory(&section) != 0)
			panic("Unable to map region %08x [%d bytes]\n",
			      section.VirtualAddress, section.Size);

		ret = SeekFD(fd, (s32)programHeader->p_offset, 0);
		if (ret < 0)
			goto cleanup_launch;

		ret = ReadFD(fd, (void *)programHeader->p_vaddr, programHeader->p_filesz);
		if (ret != (s32)programHeader->p_filesz)
			goto cleanup_launch;

		//if the filecontent < the memory size we need to clear it
		if (ret < (s32)programHeader->p_memsz)
			memset((void *)programHeader->p_vaddr, 0, programHeader->p_memsz - (u32)ret);

		//unknown flags
		switch (programHeader->p_flags)
		{
			case 2:
				section.AccessRights = AP_RWUSER;
				break;
			case 0:
				section.AccessRights = AP_ROM;
				break;
			case 1:
			default:
				section.AccessRights = AP_ROUSER;
				break;
		}

		section.Domain = FLAGSTODOMAIN(programHeader->p_flags << 6);
		if (MapMemory(&section) != 0)
			panic("Unable to map region %08x [%d bytes]\n",
			      section.VirtualAddress, section.Size);

		//map virtual address
		section.VirtualAddress = MEM2_PHY2VIRT(section.VirtualAddress);
		section.IsCached = 0;
		if (MapMemory(&section) != 0)
			panic("Unable to map region %08x [%d bytes]\n",
			      section.VirtualAddress, section.Size);

		//no idea why but hey, you do you IOS
		ret = elfHeader->e_phnum;
	}

	ret = 0;
	DCFlushAll();
	ICInvalidateAll();
	AhbFlushFrom(AHB_1);
	AhbFlushTo(AHB_1);

	if (noteLength == 0)
		goto cleanup_launch;

	ret = SeekFD(fd, (s32)noteOffset, 0);
	if (ret < 0)
		goto cleanup_launch;

	ret = ReadFD(fd, noteHeader, noteLength);
	if (ret != (s32)noteLength)
		goto cleanup_launch;

	ret = 0;
	const u32 noteSize = noteHeader->n_descsz / sizeof(ModuleInfo);
	const ModuleInfo *module = (ModuleInfo *)(((u32)noteHeader) + sizeof(Elf32_Nhdr));
	for (u32 index = 0; index < noteSize; index++)
	{
		s32 threadId =
		    CreateThread(module[index].EntryPoint, (void *)module[index].UserId,
		                 (u32 *)module[index].StackAddress,
		                 module[index].StackSize, module[index].Priority, 1);
		Threads[threadId].ProcessId = module[index].UserId;

		//wtf IOS?
		u32 currentProcessId = CurrentThread->ProcessId;
		CurrentThread->ProcessId = 0;
		StartThread(threadId);
		CurrentThread->ProcessId = currentProcessId;
	}

cleanup_launch:
	if (fd >= 0)
		CloseFD(fd);

	if (programHeaders)
		FreeOnHeap(0, programHeaders);

	if (elfHeader)
		FreeOnHeap(0, elfHeader);

	if (noteHeader)
		FreeOnHeap(0, noteHeader);

	return ret;
}

#endif
