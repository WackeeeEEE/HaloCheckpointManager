﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using HCM3.Services;
using HCM3.Services.Trainer;
using Microsoft.Extensions.DependencyInjection;
using BurntMemory;
using System.Diagnostics;
namespace HCM3
{
    public partial class App : Application
    {
        private void Application_Exit(object? sender, ExitEventArgs? e)
        {

            try
            {
                var PersistentCheatService = _serviceProvider.GetService<PersistentCheatService>();
                PersistentCheatService.RemoveAllCheats();


                var TrainerServices = _serviceProvider.GetService<TrainerServices>();
                TrainerServices.RemoveAllPatches();

            }
            catch (Exception ex)
            {
                Trace.WriteLine("Failed removing cheats or patches - probably MCC was closed. ex: " + ex.ToString());
            }

            var HotkeyManager = _serviceProvider.GetService<HotkeyManager>();
            HotkeyManager.SerializeBindings();


            Trace.WriteLine("Attempting application exit");
            try
            {
                var HaloMemoryService = _serviceProvider.GetService<HaloMemoryService>();

                var DataPointersService = _serviceProvider.GetService<DataPointersService>();
                ReadWrite.Pointer? PresentPointer = (ReadWrite.Pointer)DataPointersService.GetPointer("PresentPointer_" + HaloMemoryService.HaloState.MCCType, HaloMemoryService.HaloState.CurrentAttachedMCCVersion);
                if (PresentPointer == null) throw new Exception("Couldn't get PresentPointer");
                IntPtr? PresentPointerResPtr = HaloMemoryService.ReadWrite.ResolvePointer(PresentPointer);
                if (PresentPointerResPtr == null) throw new Exception("Couldn't resolve PresentPointer");
                UInt64 PresentPointerRes = (UInt64)PresentPointerResPtr.Value.ToInt64();
                Trace.WriteLine("DISABLING PRESENT HOOK: resolved present point: " + PresentPointerRes.ToString("X"));
                var InternalServices = _serviceProvider.GetService<InternalServices>();
                InternalServices.CallInternalFunction("RemovePresentHook", PresentPointerRes);
                Trace.WriteLine("Succesfully exited application");
            }
            catch (Exception ex)
            { 
            Trace.WriteLine("error shutting down: " + ex.ToString());
            }


            this.Logger.Flush();
        }
    }
}
