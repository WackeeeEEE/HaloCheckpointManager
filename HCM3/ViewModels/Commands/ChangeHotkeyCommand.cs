﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;
using System.Diagnostics;

namespace HCM3.ViewModels.Commands
{
    public class ChangeHotkeyCommand : ICommand
    {

        public ChangeHotkeyCommand(ActionControlViewModel actionControlViewModel)
        {
            this.ActionControlViewModel = actionControlViewModel;

        }

        private ActionControlViewModel ActionControlViewModel { get; init; }
        public bool CanExecute(object? parameter)
        {
            return true;
        }

        public void Execute(object? parameter)
        {

            ActionControlViewModel.HotkeyText = "ya changed";
            Trace.WriteLine("User commanded ChangeHotkey, parameter: " + parameter?.ToString());
            Trace.WriteLine("HOTKEY: " + ActionControlViewModel.HotkeyText);
            //ActionControlViewModel.OnHotkeyPress(); this is the arg we want to hotkey to execute


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
