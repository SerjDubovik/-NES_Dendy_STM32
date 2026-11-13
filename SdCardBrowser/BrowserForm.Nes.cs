namespace SdCardBrowser;

internal sealed partial class BrowserForm
{
    // Bit order matches the NES controller's serial register.
    private static readonly string[] NesButtons = ["A", "B", "Select", "Start", "↑", "↓", "←", "→"];
    private static readonly Keys[] DefaultNesKeys =
        [Keys.X, Keys.Z, Keys.Back, Keys.Enter, Keys.Up, Keys.Down, Keys.Left, Keys.Right];
    private readonly TabControl _browserTabs = new() { Dock = DockStyle.Fill };
    private readonly TabPage _nesTab = new("Управление NES");
    private readonly Button _nesReturn = new() { AutoSize = true, Text = "В меню" };
    private readonly Button _nesPlay = new() { AutoSize = true, Text = "Включить управление" };
    private readonly Label _nesStatus = new() { AutoSize = true, MaximumSize = new Size(660, 0) };
    private readonly Button[] _nesBindings = new Button[NesButtons.Length];
    private readonly System.Windows.Forms.Timer _nesTimer = new() { Interval = 25 };
    private readonly Queue<byte> _nesPending = new();
    private Keys[] _nesKeys = (Keys[])DefaultNesKeys.Clone();
    private int _nesAssign = -1;
    private bool _nesPlaying;
    private Task? _nesExchange;
    private byte _nesMask;
    private long _nesLastSend;

    private void InitializeNesTab()
    {
        var filesTab = new TabPage("Файлы");
        layout.Controls.Remove(panes);
        filesTab.Controls.Add(panes);
        _browserTabs.TabPages.Add(filesTab);
        _browserTabs.TabPages.Add(_nesTab);
        layout.Controls.Add(_browserTabs, 0, 1);

        var rows = new TableLayoutPanel
        {
            AutoSize = true, ColumnCount = 2, Padding = new Padding(16), Dock = DockStyle.Top
        };
        rows.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 90));
        rows.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        _nesTab.AutoScroll = true;
        _nesTab.Controls.Add(rows);
        for (int i = 0; i < NesButtons.Length; i++)
        {
            int index = i;
            var label = new Label { Text = NesButtons[i], AutoSize = true, Anchor = AnchorStyles.Left };
            var binding = new Button
            {
                AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink,
                MinimumSize = new Size(152, 30),
                Padding = new Padding(8, 3, 8, 3)
            };
            binding.Click += (_, _) =>
            {
                StopNesInput();
                _nesAssign = index;
                UpdateNesBindings();
                _nesStatus.Text = "Нажмите новую клавишу. Esc отменяет назначение.";
            };
            _nesBindings[i] = binding;
            rows.Controls.Add(label, 0, i);
            rows.Controls.Add(binding, 1, i);
        }

        var actions = new FlowLayoutPanel { AutoSize = true, WrapContents = false };
        var reset = new Button { AutoSize = true, Text = "По умолчанию" };
        reset.Click += (_, _) =>
        {
            StopNesInput();
            _nesAssign = -1;
            _nesKeys = (Keys[])DefaultNesKeys.Clone();
            UpdateNesBindings();
            if (SaveSettings()) _nesStatus.Text = "Привязки сохранены.";
        };
        actions.Controls.AddRange([_nesPlay, _nesReturn, reset]);
        rows.Controls.Add(actions, 0, NesButtons.Length);
        rows.SetColumnSpan(actions, 2);
        rows.Controls.Add(_nesStatus, 0, NesButtons.Length + 1);
        rows.SetColumnSpan(_nesStatus, 2);

        _nesReturn.Click += async (_, _) => await ReturnToNesMenuAsync();
        _nesPlay.Click += (_, _) =>
        {
            if (_nesPlaying) StopNesInput();
            else
            {
                _nesAssign = -1;
                _nesPlaying = true;
                _nesPending.Enqueue(0);
                _nesStatus.Text = "Управление активно. Esc останавливает передачу.";
                UpdateNesBindings();
                UpdateControls();
            }
        };
        if (_settings.ShowNesTab) _browserTabs.SelectedTab = _nesTab;
        _browserTabs.SelectedIndexChanged += (_, _) =>
        {
            LeaveNesInput();
            _settings.ShowNesTab = _browserTabs.SelectedTab == _nesTab;
            SaveSettings();
        };
        Deactivate += (_, _) => LeaveNesInput();
        _nesTimer.Tick += async (_, _) => await SendNesInputAsync();
        Disposed += (_, _) => _nesTimer.Dispose();
        _nesTimer.Start();
        UpdateNesBindings();
    }

    private async Task ReturnToNesMenuAsync()
    {
        if (_client is not { SupportsNesMenu: true } client) return;
        bool returned = await RunAsync(async () =>
        {
            await Task.Run(client.ReturnToNesMenu);
            _nesPending.Clear();
            _nesMask = 0;
        }, keepNesInput: true);
        if (returned) statusLabel.Text = _nesStatus.Text = "На плате открыт выбор режима.";
    }

    private static bool IsNesKey(Keys key) => key > Keys.XButton2 && key <= Keys.OemClear &&
        Enum.IsDefined(key) && key is not (Keys.Escape or Keys.LWin or Keys.RWin or Keys.Menu or Keys.LMenu or Keys.RMenu);

    private void UpdateNesBindings()
    {
        for (int i = 0; i < NesButtons.Length; i++)
            _nesBindings[i].Text = i == _nesAssign ? "Нажмите клавишу…" : _nesKeys[i].ToString();
    }

    private void LeaveNesInput()
    {
        StopNesInput();
        if (_nesAssign < 0) return;
        _nesAssign = -1;
        UpdateNesBindings();
        _nesStatus.Text = "Назначение отменено.";
    }

    private void StopNesInput()
    {
        if (!_nesPlaying) return;
        _nesPlaying = false;
        _nesMask = 0;
        _nesPending.Clear();
        _nesPending.Enqueue(0);
        _nesStatus.Text = "Управление остановлено.";
        UpdateControls();
    }

    private bool FilterNesKey(ref Message message)
    {
        if (_browserTabs.SelectedTab != _nesTab || ActiveForm != this ||
            message.Msg is not (0x0100 or 0x0101) ||
            Control.FromChildHandle(message.HWnd)?.FindForm() != this)
            return false;

        var key = (Keys)(int)message.WParam;
        bool down = message.Msg == 0x0100;
        bool repeat = (message.LParam.ToInt64() & (1L << 30)) != 0;
        if (_nesAssign >= 0)
        {
            if (!down || repeat) return true;
            if (key != Keys.Escape)
            {
                if (!IsNesKey(key)) return true;
                int existing = Array.IndexOf(_nesKeys, key);
                if (existing >= 0 && existing != _nesAssign)
                    _nesKeys[existing] = _nesKeys[_nesAssign];
                _nesKeys[_nesAssign] = key;
                if (SaveSettings()) _nesStatus.Text = "Привязки сохранены.";
            }
            else _nesStatus.Text = "Назначение отменено.";
            _nesAssign = -1;
            UpdateNesBindings();
            return true;
        }

        if (!_nesPlaying) return false;
        if (key == Keys.Escape)
        {
            if (down) StopNesInput();
            return true;
        }

        int index = Array.IndexOf(_nesKeys, key);
        if (index < 0) return false;
        if (down && repeat) return true;
        byte bit = (byte)(1 << index);
        byte mask = down ? (byte)(_nesMask | bit) : (byte)(_nesMask & ~bit);
        if (mask != _nesMask)
        {
            _nesMask = mask;
            // Limit latency if the serial link temporarily stalls.
            if (_nesPending.Count == 16) _nesPending.Clear();
            _nesPending.Enqueue(mask);
        }
        return true;
    }

    private async Task SendNesInputAsync()
    {
        if (_busy || _nesExchange is not null || _client is not { SupportsNesInput: true } client) return;
        if (!_nesPending.TryDequeue(out byte mask))
        {
            if (!_nesPlaying || _nesMask == 0 || Environment.TickCount64 - _nesLastSend < 100) return;
            mask = _nesMask;
        }

        _nesLastSend = Environment.TickCount64;
        try
        {
            _nesExchange = Task.Run(() => client.SetNesInput(mask));
            await _nesExchange;
        }
        catch (IOException ex)
        {
            Disconnect();
            _nesStatus.Text = ex.Message;
        }
        finally
        {
            _nesExchange = null;
            UpdateControls();
            if (_closeRequested && !_busy) BeginInvoke(Close);
        }
    }
}
