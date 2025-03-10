﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using HCM3.Helpers;
using BurntMemory;
using System.Diagnostics;

namespace HCM3.Services.Trainer
{
    public partial class TrainerServices
    {
        // This if for patches that don't actually modify game behaviour, but are useful to the function of HCM
        // Example: patching the game so it doesn't check the checksums on checkpoints (allows people to load checkopints made by other players)


        public void SetupPatches()
        {
            listOfPatches = new();


            listOfPatches.Add("H2PlayerData", new PatchStateObject("H2", "H2_PlayerData_DetourInfo", true));
            listOfPatches.Add("H3PlayerData", new PatchStateObject("H3", "H3_PlayerData_DetourInfo", true));
            listOfPatches.Add("ODPlayerData", new PatchStateObject("OD", "OD_PlayerData_DetourInfo", true));
            listOfPatches.Add("HRPlayerData", new PatchStateObject("HR", "HR_PlayerData_DetourInfo", true));

            listOfPatches.Add("H3ChecksumFix", new PatchStateObject("H3", "H3_ChecksumFix_PatchInfo", false));
            listOfPatches.Add("ODChecksumFix", new PatchStateObject("OD", "OD_ChecksumFix_PatchInfo", false));
            listOfPatches.Add("HRChecksumFix", new PatchStateObject("HR", "HR_ChecksumFix_PatchInfo", false));
            listOfPatches.Add("H4ChecksumFix", new PatchStateObject("H4", "H4_ChecksumFix_PatchInfo", false));

        }

        private bool errorShown = false;

        private object lockApplyPatches = new object();
        public void ApplyPatches(object? sender, HaloStateEvents.HaloStateChangedEventArgs e)
        {
            lock (lockApplyPatches)
            {
                int loadedGame = e.NewHaloState;
                string gameAs2Letters = Dictionaries.GameTo2LetterGameCode[(int)loadedGame];



                foreach (KeyValuePair<string, PatchStateObject> kvp in listOfPatches)
                {
                    try
                    {
                        if (gameAs2Letters == kvp.Value.Game)
                        {
                            Trace.WriteLine("Found patch (" + kvp.Key + ") that we want to make sure is applied.");
                            if (kvp.Value.IsDetour)
                            {
                               
                                DetourInfoObject detourInfoObject = (DetourInfoObject)this.CommonServices.GetRequiredPointers(kvp.Value.PointerName);
                                bool originalCodeIsOriginal = this.PersistentCheatService.DetourCheckOG(detourInfoObject);


                                if (!originalCodeIsOriginal)
                                {
                                    IntPtr? handleToPreviousDetour = null;
                                    if (kvp.Value.Applied)
                                    {
                                        handleToPreviousDetour = kvp.Value.PatchHandle;
                                    }
                                    this.PersistentCheatService.DetourRemove(detourInfoObject, handleToPreviousDetour);
                                }


                                IntPtr detourHandle = this.PersistentCheatService.DetourApply(detourInfoObject);
                                kvp.Value.PatchHandle = detourHandle;
                                kvp.Value.Applied = true;
                            }
                            else //simple patch
                            {

                                if (!IsPatchApplied(kvp.Key, false))
                                {
                                    Trace.WriteLine("Patch (" + kvp.Key + ") wasn't applied so applying now.");
                                    ApplyPatch(kvp.Key, false);
                                }
                                kvp.Value.Applied = true;
                            }

                        }
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine("Error applying patch (" + kvp.Key + "): " + ex.ToString());
                        if (!errorShown)
                        {
                            errorShown = true;
                            System.Windows.MessageBox.Show("An error occured while applying a HCM patch (" + kvp.Key + "), this may cause some cheat functions to not work correctly. Error: \n\n" + ex.ToString());
                        }

                    }
                }
            }
 
        }

        public Dictionary<string, PatchStateObject> listOfPatches { get; set; }


        private object lockApplyPatch = new object();
        public void ApplyPatch(string patchName, bool isDetour)
        {
            lock (lockApplyPatch)
            {
                PatchStateObject ourPatch = listOfPatches[patchName];
                if (isDetour)
                {

                    DetourInfoObject detourInfoObject = (DetourInfoObject)this.CommonServices.GetRequiredPointers(ourPatch.PointerName);
                    bool originalCodeIsOriginal = this.PersistentCheatService.DetourCheckOG(detourInfoObject);


                    if (!originalCodeIsOriginal)
                    {
                        this.PersistentCheatService.DetourRemove(detourInfoObject, null);
                    }

                    IntPtr detourHandle = this.PersistentCheatService.DetourApply(detourInfoObject);
                    ourPatch.PatchHandle = detourHandle;
                    ourPatch.Applied = true;
                }
                else //simple patch
                {
                    PatchInfo patchInfo = (PatchInfo)this.CommonServices.GetRequiredPointers(ourPatch.PointerName);
                    this.HaloMemoryService.ReadWrite.WriteBytes(patchInfo.OriginalCodeLocation, patchInfo.PatchedCodeBytes, true);
                }
            }
        }

        public bool IsPatchApplied(string patchName, bool isDetour)
        {
            PatchStateObject ourPatch = listOfPatches[patchName];

            if (isDetour)
            {
                DetourInfoObject detourInfoObject = (DetourInfoObject)this.CommonServices.GetRequiredPointers(ourPatch.PointerName);
                return !this.PersistentCheatService.DetourCheckOG(detourInfoObject);
            }
            else //simple patch
            {
                PatchInfo patchInfo = (PatchInfo)this.CommonServices.GetRequiredPointers(ourPatch.PointerName);
                bool codeIsPatched = true;

                byte[] currentCode = this.HaloMemoryService.ReadWrite.ReadBytes(patchInfo.OriginalCodeLocation, patchInfo.OriginalCodeBytes.Length);

                for (int i = 0; i < currentCode.Length; i++)
                {
                    if (currentCode[i] != patchInfo.PatchedCodeBytes[i])
                    {
                        codeIsPatched = false;
                        break;
                    }
                }
                return codeIsPatched;
            }
        }

        public void RemovePatch(string patchName, bool isDetour)
        {
            PatchStateObject ourPatch = listOfPatches[patchName];

            if (isDetour)
            {
                DetourInfoObject detourInfoObject = (DetourInfoObject)this.CommonServices.GetRequiredPointers(ourPatch.PointerName);
                bool originalCodeIsOriginal = this.PersistentCheatService.DetourCheckOG(detourInfoObject);

                if (!originalCodeIsOriginal)
                {
                    this.PersistentCheatService.DetourRemove(detourInfoObject, ourPatch.PatchHandle);
                }

                ourPatch.Applied = false;
            }
            else //simple patch
            {
                PatchInfo patchInfo = (PatchInfo)this.CommonServices.GetRequiredPointers(ourPatch.PointerName);
                this.HaloMemoryService.ReadWrite.WriteBytes(patchInfo.OriginalCodeLocation, patchInfo.OriginalCodeBytes, true);
            }
        }

        //only run on HCM shutdown
        public void RemoveAllPatches()
        {
            this.HaloMemoryService.HaloState.UpdateHaloState();
            int loadedGame = this.HaloMemoryService.HaloState.CurrentHaloState;
            string gameAs2Letters = Dictionaries.GameTo2LetterGameCode[(int)loadedGame];

            foreach (KeyValuePair<string, PatchStateObject> kvp in listOfPatches)
            {
                try
                {
                    if (gameAs2Letters == kvp.Value.Game)
                    {
                        DetourInfoObject detourInfoObject = (DetourInfoObject)this.CommonServices.GetRequiredPointers(kvp.Value.PointerName);
                        bool originalCodeIsOriginal = this.PersistentCheatService.DetourCheckOG(detourInfoObject);

                        if (!originalCodeIsOriginal)
                        {
                            this.PersistentCheatService.DetourRemove(detourInfoObject, null);
                        }

                        kvp.Value.Applied = false;


                    }
                }
                catch (Exception ex)
                {
                    Trace.WriteLine("Error removing patch: " + ex.ToString());
                }
            }


        }


    }
}
