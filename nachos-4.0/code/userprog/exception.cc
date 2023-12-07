// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "main.h"
#include "syscall.h"
#include "machine.h"

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void
ExceptionHandler(ExceptionType which)
{
	int	type = kernel->machine->ReadRegister(2);
	int	val;

    switch (which) {
	case SyscallException:
	    switch(type) {
		case SC_Halt:
		    DEBUG(dbgAddr, "Shutdown, initiated by user program.\n");
   		    kernel->interrupt->Halt();
		    break;
		case SC_PrintInt:
			val=kernel->machine->ReadRegister(4);
			cout << "Print integer:" <<val << endl;
			return;
		case SC_Sleep:
			val=kernel->machine->ReadRegister(4);
			cout << "Sleep for:" << val << endl;
			kernel->alarm->WaitUntil(val);
			return;
/*		case SC_Exec:
			DEBUG(dbgAddr, "Exec\n");
			val = kernel->machine->ReadRegister(4);
			kernel->StringCopy(tmpStr, retVal, 1024);
			cout << "Exec: " << val << endl;
			val = kernel->Exec(val);
			kernel->machine->WriteRegister(2, val);
			return;
*/		case SC_Exit:
			DEBUG(dbgAddr, "Program exit\n");
			val=kernel->machine->ReadRegister(4);
			cout << "return value:" << val << endl;
			kernel->currentThread->Finish();
			break;
		default:
		    cerr << "Unexpected system call " << type << "\n";
 		    break;
	    }
	    break;
	case PageFaultException:		
		kernel->stats->numPageFaults++;  // record page fault
		unsigned int vpn = (unsigned) kernel->machine->ReadRegister(BadVAddrReg) / PageSize;  // load the page # failed to find
		// see if there is spare physical page
		unsigned int phy_mem_idx = 0;
		while (kernel->machine->PhyMemStatus[phy_mem_idx] == true && phy_mem_idx < NumPhysPages)
		{
			phy_mem_idx++;
		}

		if (phy_mem_idx < NumPhysPages)  // there is available physical page
		{
			char *read_disk_buffer;
			read_disk_buffer = new char[PageSize];
			kernel->machine->PhyMemStatus[phy_mem_idx] = true;
			kernel->machine->phys_pages[phy_mem_idx] = &(kernel->machine->pageTable[vpn]);
			kernel->machine->pageTable[vpn].valid = true;
			kernel->machine->pageTable[vpn].physicalPage = phy_mem_idx;
			kernel->machine->pageTable[vpn].access_times++;  // record calling of the page
			// load the page in
			kernel->secondMem->ReadSector(kernel->machine->pageTable[vpn].virtualPage, read_disk_buffer);
			bcopy(read_disk_buffer, &(kernel->machine->mainMemory[phy_mem_idx*PageSize]), PageSize);
		}
		else  // select a physical page to swap
		{			
			// find the least used physical page
			unsigned int min_usage = kernel->machine->phys_pages[0].access_times;
			int victim_page_idx = 0;
			for (int i = 1; i < NumPhysPages; i++)
			{
				if (kernel->machine->phys_pages[i].access_times < min_usage)
				{
					min_usage = kernel->machine->phys_pages[i].access_times;
					victim_page_idx = i;
				}				
			}
			kernel->machine->phys_pages[victim_page_idx] = 1;  // zero original value + access once

			char *read_disk_buffer;
			char *read_mem_buffer;
			read_disk_buffer = new char[PageSize];
			read_mem_buffer = new char[PageSize];
			// read old page from memory
			bcopy(&(kernel->machine->mainMemory[victim_page_idx*PageSize]), read_mem_buffer, PageSize);
			// read new page from disk
			kernel->secondMem->ReadSector(kernel->machine->pageTable[vpn].virtualPage, read_disk_buffer);
			// write new page into memory
			bcopy(read_disk_buffer, &(kernel->machine->mainMemory[victim_page_idx*PageSize]), PageSize);
			// write old page into disk
			kernel->secondMem->WriteSector(kernel->machine->pageTable[vpn].virtualPage, read_mem_buffer);

			// update info of swapped page
			kernel->machine->phys_pages[victim_page_idx]->virtualPage = kernel->machine->pageTable[vpn].virtualPage;
			kernel->machine->phys_pages[victim_page_idx]->valid = false;

			//update info of loaded page
			kernel->machine->pageTable[vpn].valid = true;
			kernel->machine->pageTable[vpn].physicalPage = victim_page_idx;
			kernel->machine->phys_pages[victim_page_idx] = &(kernel->machine->pageTable[vpn]);
		}
		return;
	default:
	    cerr << "Unexpected user mode exception" << which << "\n";
	    break;
    }
    ASSERTNOTREACHED();
}
