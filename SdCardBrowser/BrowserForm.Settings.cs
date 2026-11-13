using System.Text.Json;
using System.Text.Json.Serialization;

namespace SdCardBrowser;

internal sealed partial class BrowserForm
{
    private sealed class BrowserSettings
    {
        public string? Port { get; set; }
        public int BaudRate { get; set; } = 115200;
        public bool UseFlash { get; set; }
        public Dictionary<string, Keys>? NesKeys { get; set; }
        public bool ShowNesTab { get; set; }
        public string? PcFolder { get; set; }
        public int WindowWidth { get; set; }
        public int WindowHeight { get; set; }
        public bool Maximized { get; set; }
        public double SplitterRatio { get; set; }
        public int[]? BoardColumns { get; set; }
        public int[]? PcColumns { get; set; }
    }

    private static readonly string SettingsFolder = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "SdCardBrowser");
    private static readonly string SettingsPath = Path.Combine(SettingsFolder, "settings.json");
    private static readonly JsonSerializerOptions SettingsJson = new()
    {
        WriteIndented = true,
        Converters = { new JsonStringEnumConverter<Keys>(allowIntegerValues: false) }
    };

    private BrowserSettings _settings = new();

    private void LoadSettings()
    {
        try
        {
            if (File.Exists(SettingsPath))
            {
                _settings = JsonSerializer.Deserialize<BrowserSettings>(File.ReadAllText(SettingsPath), SettingsJson)
                    ?? throw new JsonException();
            }
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or JsonException)
        {
            _nesStatus.Text = "Не удалось прочитать настройки. Загружены значения по умолчанию.";
        }

        if (_settings.NesKeys is null)
        {
            string oldPath = Path.Combine(SettingsFolder, "nes-keys.json");
            try
            {
                if (File.Exists(oldPath))
                    _settings.NesKeys = JsonSerializer.Deserialize<Dictionary<string, Keys>>(File.ReadAllText(oldPath), SettingsJson);
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or JsonException)
            {
                _nesStatus.Text = "Не удалось прочитать привязки. Загружены клавиши по умолчанию.";
            }
        }

        if (_settings.NesKeys is { } keys)
        {
            Keys[] saved = NesButtons.Select(button => keys.GetValueOrDefault(button)).ToArray();
            if (saved.All(IsNesKey) && saved.Distinct().Count() == NesButtons.Length)
                _nesKeys = saved;
            else
                _nesStatus.Text = "Не удалось прочитать привязки. Загружены клавиши по умолчанию.";
        }
    }

    private bool SaveSettings()
    {
        _settings.NesKeys = NesButtons.Select((button, i) => (button, key: _nesKeys[i]))
            .ToDictionary(binding => binding.button, binding => binding.key);
        string temporary = SettingsPath + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            Directory.CreateDirectory(SettingsFolder);
            File.WriteAllText(temporary, JsonSerializer.Serialize(_settings, SettingsJson));
            File.Move(temporary, SettingsPath, overwrite: true);
            return true;
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            statusLabel.Text = _nesStatus.Text = $"Не удалось сохранить настройки: {ex.Message}";
            return false;
        }
        finally
        {
            try { File.Delete(temporary); }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { }
        }
    }

    private void RestoreWindowLayout()
    {
        if (_settings.WindowWidth > 0 && _settings.WindowHeight > 0)
        {
            Rectangle area = Screen.FromControl(this).WorkingArea;
            Size = new Size(
                Math.Clamp(_settings.WindowWidth, MinimumSize.Width, Math.Max(MinimumSize.Width, area.Width)),
                Math.Clamp(_settings.WindowHeight, MinimumSize.Height, Math.Max(MinimumSize.Height, area.Height)));
        }
        if (_settings.Maximized) WindowState = FormWindowState.Maximized;

        RestoreColumnWidths(filesListView, _settings.BoardColumns);
        RestoreColumnWidths(pcListView, _settings.PcColumns);
    }

    private void RestoreSplitter()
    {
        int maximum = panes.ClientSize.Width - panes.Panel2MinSize - panes.SplitterWidth;
        if (_settings.SplitterRatio is > 0 and < 1 && maximum >= panes.Panel1MinSize)
        {
            int distance = (int)Math.Round(panes.ClientSize.Width * _settings.SplitterRatio);
            panes.SplitterDistance = Math.Clamp(distance, panes.Panel1MinSize, maximum);
        }
    }

    private static void RestoreColumnWidths(ListView list, int[]? widths)
    {
        if (widths?.Length != list.Columns.Count) return;
        for (int i = 0; i < widths.Length; i++)
        {
            if (widths[i] is >= 0 and <= 8192) list.Columns[i].Width = widths[i];
        }
    }

    private void SaveWindowLayout()
    {
        Size normalSize = WindowState == FormWindowState.Normal ? Size : RestoreBounds.Size;
        _settings.WindowWidth = normalSize.Width;
        _settings.WindowHeight = normalSize.Height;
        if (WindowState != FormWindowState.Minimized)
            _settings.Maximized = WindowState == FormWindowState.Maximized;
        if (panes.ClientSize.Width > 0)
            _settings.SplitterRatio = (double)panes.SplitterDistance / panes.ClientSize.Width;
        _settings.BoardColumns = filesListView.Columns.Cast<ColumnHeader>().Select(column => column.Width).ToArray();
        _settings.PcColumns = pcListView.Columns.Cast<ColumnHeader>().Select(column => column.Width).ToArray();
        SaveSettings();
    }
}
