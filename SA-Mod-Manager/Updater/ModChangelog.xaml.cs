﻿using SAModManager.Updater;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Collections.ObjectModel;
using System.Diagnostics;

namespace SAModManager.Updater
{
    /// <summary>
    /// Interaction logic for ModChangelog.xaml
    /// </summary>

    public partial class ModChangelog : Window
    {
        public class ModChangeLogData
        {
            public string filename { get; set; }
            public string status { get; set; }
        }

        ObservableCollection<ModChangeLogData> modchangeData { get; set; } = new();
        private readonly List<ModDownload> mods;

        public List<ModDownload> SelectedMods { get; } = new();

        public void SetData(ModDownload entry)
        {
            if (entry == null)
            {
                // Download details
                FileSize.Text = null;
                FileCount.Text = null;
                // Release details	
                UpdateName.Text = null;
                UpdateTag.Text = null;

            }
            else
            {
                // Download details
                PublishedDate.Text = entry.Updated.ToString(CultureInfo.CurrentCulture);
                FileSize.Text = SizeSuffix.GetSizeSuffix(entry.Size);
                FileCount.Text = entry.FilesToDownload.ToString();

                UpdateName.Text = entry.Name;
                UpdateTag.Text = entry.Version;
            }

            BtnOpenUpdate.IsEnabled = !string.IsNullOrEmpty(entry.ReleaseUrl);
        }

        private void SetModDetails(ModDownload entry)
        {
            textChangeLog.Text = entry?.Changes.Trim();
            SetData(entry);

            FilesList.BeginInit();

            if (entry?.Type.Equals(ModDownloadType.Modular) == true)
            {
                tabPageFiles.Visibility = Visibility.Visible;
                tabPageFiles.IsEnabled = true;

                foreach (ModManifestDiff i in entry.ChangedFiles)
                {
                    string file = i.State == ModManifestState.Moved ? $"{i.Last.FilePath} -> {i.Current.FilePath}" : i.Current.FilePath;

                    ModChangeLogData data = new()
                    {
                        filename = file,
                        status = i.State.ToString(),
                    };

                    modchangeData.Add(data);
                }

                // Auto-resize columns based on content
                if (FilesList.View is GridView gridView)
                {
                    foreach (GridViewColumn column in gridView.Columns)
                    {
                        column.Width = 0;
                        column.Width = double.NaN;
                    }
                }
            }
            else
            {
                tabPageFiles.Visibility = Visibility.Collapsed;
                tabPageFiles.IsEnabled = false;
            }

            FilesList.ItemsSource = modchangeData;
            FilesList.EndInit();

            DataContext = modchangeData;
        }

        private void AdjustDetailsDisplay(ModDownload mod)
        {
            bool IsSelfHosted = string.IsNullOrEmpty(mod.Info.UpdateUrl);

            if (IsSelfHosted)
            {
                UpdateDetails.RowDefinitions[2].Height = new GridLength(30);
                UpdateDetails.RowDefinitions[3].Height = new GridLength(0);
                UpdateDetails.RowDefinitions[4].Height = new GridLength(0);
            }
            else
            {
                UpdateDetails.RowDefinitions[2].Height = new GridLength(0);
                UpdateDetails.RowDefinitions[3].Height = new GridLength(30);
                UpdateDetails.RowDefinitions[4].Height = new GridLength(30);
            }
        }

        public ModChangelog(List<ModDownload> mods)
        {
            InitializeComponent();
            this.mods = mods;
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            ModsList.BeginInit();

            foreach (ModDownload download in mods)
            {
                download.IsChecked = true;
                ModsList.Items.Add(download);
            }

            ModsList.SelectedIndex = 0;

            ModsList.EndInit();
        }

        private void DLButton_Click(object sender, RoutedEventArgs e)
        {
            foreach (ModDownload item in mods)
            {
                if (item.IsChecked == true)
                    SelectedMods.Add(item);
            }

            DialogResult = true;
        }

        private void CheckBox_Checked(object sender, RoutedEventArgs e)
        {
            btnInstallNow.IsEnabled = ModsList.Items.Cast<ModDownload>().Any(x => x.IsChecked);
        }

        private void ModsList_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            var items = ModsList.SelectedItems;

            if (items.Count > 1 || items.Count == 0)
            {
                SetModDetails(null);
            }
            else
            {
                var entry = items[0] as ModDownload;
                SetModDetails(entry);
                AdjustDetailsDisplay(entry);
            }
        }

        private void ImageButton_Click_1(object sender, RoutedEventArgs e)
        {
            DialogResult = false;
            this.Close();
        }

        private void BtnOpenUpdate_Click(object sender, RoutedEventArgs e)
        {
            var item = ModsList.SelectedItem;

            var entry = item as ModDownload;

            if (entry != null)
            {
                var ps = new ProcessStartInfo(entry.ReleaseUrl)
                {
                    UseShellExecute = true,
                    Verb = "open"
                };
                Process.Start(ps);
            }
        }
    }
}
