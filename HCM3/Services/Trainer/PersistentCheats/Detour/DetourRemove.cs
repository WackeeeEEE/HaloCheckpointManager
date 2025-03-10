﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using BurntMemory;
using HCM3.Helpers;
using System.Diagnostics;

namespace HCM3.Services.Trainer
{ 
    public partial class PersistentCheatService
    {
        public void DetourRemove(DetourInfoObject detourInfo, IntPtr? detourHandle)
        { 


                // Need to restore original bytes at hook location
                this.HaloMemoryService.ReadWrite.WriteBytes(detourInfo.OriginalCodeLocation, detourInfo.OriginalCodeBytes, true);


            // Deallocate previous memory
            if (detourHandle != null)
            {
                // Deallocate previous detourhandle, failure doesn't really matter
                try
                {
                    Process MCCProcess = Process.GetProcessById((int)this.HaloMemoryService.HaloState.ProcessID);
                    PInvokes.VirtualFreeEx(MCCProcess.Handle, detourHandle.Value, detourInfo.SizeToAlloc, PInvokes.AllocationType.Release);
                    Trace.WriteLine("Successfully deallocated detour page at " + detourHandle.Value.ToString("X"));
                }
                catch
                {
                    Trace.WriteLine("Failed deallocating previous detourHandle; don't really care. Though if this happens a lot will cause issues");
                }
            }
            else
            {
                Trace.WriteLine("Couldn't deallocate a removed detour as the handle to it was null");
            }
        }

    }
}
