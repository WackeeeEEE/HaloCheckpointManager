﻿using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using HCM3.Views;
using System.Collections.ObjectModel;
using HCM3.ViewModels;
using HCM3.Models;
using Microsoft.Extensions.DependencyInjection;
using HCM3.Startup;
using System.Diagnostics;
using HCM3.Services;
using HCM3.Services.Trainer;
using HCM3.ViewModels.Commands;

namespace HCM3
{

    
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        private readonly ServiceProvider _serviceProvider;

        public App()
        {
            ServiceCollection services = new();
            ConfigureServices(services);

            _serviceProvider = services.BuildServiceProvider();


        }

        private void ConfigureServices(ServiceCollection services)
        {


            services.AddSingleton<MainWindow>();

            //ViewModels
            services.AddSingleton<MainViewModel>();
            services.AddSingleton<CheckpointViewModel>();
            services.AddSingleton<TrainerViewModel>();

            //General Services
            services.AddSingleton<CommonServices>();
            services.AddSingleton<DataPointersService>();
            services.AddSingleton<HaloMemoryService>();


            //ViewModel interaction Services
            services.AddSingleton<PersistentCheatManager>();
            services.AddSingleton<CheckpointServices>();
            services.AddSingleton<TrainerServices>();
            services.AddSingleton<PC_ToggleInvuln>();
        }

        //might have to remove sender parameter here
        private void OnStartup(object sender, StartupEventArgs e)
        {
            Trace.WriteLine("OnStartup is run");

            HCMSetup setup = new();
            // Run some checks; have admin priviledges, have file access, have required folders & files.
            if (!setup.HCMSetupChecks(out string errorMessage))
            {
                // If a check fails, tell the user why, then shutdown the application.
                System.Windows.MessageBox.Show(errorMessage, "HaloCheckpointManager Error", System.Windows.MessageBoxButton.OK);
                System.Windows.Application.Current.Shutdown();
            }

            var dataPointersService = _serviceProvider.GetService<DataPointersService>();
            // Create collection of all our ReadWrite.Pointers (and other data) and load them from the online repository
            if (!dataPointersService.LoadPointerDataFromGit(out string error))
            {
                System.Windows.MessageBox.Show(error, "HaloCheckpointManager Error", System.Windows.MessageBoxButton.OK);
                System.Windows.Application.Current.Shutdown();
            }

            // Tell HaloMemory to try to attach to MCC, both steam and winstore versions
            var haloMemoryService = _serviceProvider.GetService<HaloMemoryService>();
            this.HaloMemoryService = haloMemoryService;
            haloMemoryService.HaloState.ProcessesToAttach = new string[] { "MCC-Win64-Shipping", "MCCWinStore-Win64-Shipping" };
            haloMemoryService.HaloState.TryToAttachTimer.Enabled = true;




          
            var mainWindow = _serviceProvider.GetService<MainWindow>();
            mainWindow.DataContext = _serviceProvider.GetService<MainViewModel>();
            mainWindow.Show();
        }

        private HaloMemoryService? HaloMemoryService { get; set; }
        private void Application_Exit(object? sender, ExitEventArgs? e)
        {
            if (HaloMemoryService != null)
            {
                HaloMemoryService.DebugManager.GracefullyCloseDebugger(sender, e);
                HaloMemoryService.SpeedhackManager.RemoveSpeedHack(sender, e);

            }
        }
    }
}
