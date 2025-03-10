﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;
using HCM3.Models;
using HCM3.ViewModels;
using System.Diagnostics;
using HCM3.Services;
using HCM3.Views.Controls;
using System.Windows;

namespace HCM3.ViewModels.Commands
{
    internal class SortCheckpointCommand : ICommand
    {
        internal SortCheckpointCommand(CheckpointViewModel checkpointViewModel, CheckpointServices checkpointServices)
        {
            this.CheckpointViewModel = checkpointViewModel;
            this.CheckpointServices = checkpointServices;
        }

        public CheckpointViewModel CheckpointViewModel { get; init; }
        public CheckpointServices CheckpointServices { get; init; }


        public bool CanExecute(object? parameter)
        {
            return (CheckpointViewModel.SelectedSaveFolder != null && CheckpointViewModel.CheckpointCollection.Count > 1);
        }

        public void Execute(object? parameter)
        {
            try
            {
                Application.Current.Dispatcher.Invoke((Action)delegate {
                    SortCheckpointsView win = new(CheckpointServices, CheckpointViewModel);
                    win.Owner = App.Current.MainWindow; // makes the dialog be centers on the main window
                    win.ShowDialog();
                });


            }
            catch (Exception ex)
            {
                System.Windows.MessageBox.Show("Failed to show Sort Checkpoints window! \n" + ex.ToString(), "HaloCheckpointManager Error", System.Windows.MessageBoxButton.OK);
            }

        }

        public void RaiseCanExecuteChanged()
        {
            App.Current.Dispatcher.Invoke((Action)delegate // Need to make sure it's run on the UI thread
            {
                _canExecuteChanged?.Invoke(this, EventArgs.Empty);
            });

        }

        private EventHandler? _canExecuteChanged;

        public event EventHandler? CanExecuteChanged
        {
            add
            {
                _canExecuteChanged += value;
                CommandManager.RequerySuggested += value;
            }
            remove
            {
                _canExecuteChanged -= value;
                CommandManager.RequerySuggested -= value;
            }
        }
    }
}
