<div align="center">

# adbd for Linux

**A standalone, framework-free Android Debug Bridge daemon for Debian/Ubuntu.**

[English](README.md) · [中文](README_CN.md) · [Development guide](.github/DEVELOPMENT.md)

</div>

> **In one sentence:** this package lets a normal Linux box expose a real `adbd`
> endpoint over USB (or TCP/VSOCK), so the standard host-side `adb` and `scrcpy`
> can connect without Android framework, SurfaceFlinger, MediaCodec, or a full
> Android build tree.

## Why use it?

- **Real `adbd`, not a shim.** Builds the device-side AOSP `adbd` and installs it
  as `/usr/bin/adbd`.
- **USB gadget ready.** Includes a generic FunctionFS manager that prepares
  configfs, mounts FunctionFS, starts `adbd`, and binds the first available UDC.
- **Network modes when you need them.** Explicit TCP and VSOCK listeners are
  supported; nothing is exposed by default.
- **Useful companions.** Optional scrcpy and Android-style command shims make a
  Linux desktop feel more like an Android target.
- **No Android platform build.** A small, pinned set of upstream sources plus one
  Linux portability patch is all that is required.

## What is inside?

| Component | What it gives you |
| --- | --- |
| `adbd` | The device-side ADB daemon: shell, file sync, forwarding, and transport handling |
| `usbdevice` | USB FunctionFS gadget manager and systemd unit |
| `usbnetwork` | Optional NCM DHCP/NAT service for the adb gadget |
| `adbd-smb` | Opt-in Samba share helper; disabled by default |
| `app_process` + `scrcpy-compat` | Entry point and server used by native scrcpy 5.x |
| Android command shims | `screencap`, `screenrecord`, `input`, `getprop`, `setprop`, `wm`, `logcat`, `dumpsys`, `settings` |

## Requirements

- Linux with **configfs** and **FunctionFS** support.
- A USB port/controller that can act as a **device** (UDC), or a TCP/VSOCK setup.
- Debian/Ubuntu package management and **systemd** for the packaged service.
- Host side: `adb`; optional: `scrcpy`.
- Target side for scrcpy: `Xvfb`, `ffmpeg`, and an X11 desktop/session;
  run `scrcpy-display` (see below).
- Target side for Android-style commands: Python 3, `ffmpeg`, `x11-utils`,
  `libx11-6`, `libxtst6`, `iproute2`, `procps`.

> **Security:** `adbd` is an unauthenticated debug endpoint. Keep it on trusted
> USB, loopback, or a private network. Do not expose `tcp:5555` to untrusted
> networks.

## Install

### Debian/Ubuntu package

```bash
sudo apt install ./adbd_*.deb
```

If your package manager needs a dependency repair:

```bash
sudo dpkg -i adbd_*.deb
sudo apt-get -f install
```

The package installs `usbdevice.service` and enables it at `sysinit.target`.

### From source

```bash
git clone --recurse-submodules <repository-url>
cd adbd
sudo apt-get install build-essential debhelper cmake pkg-config patch \
    libbrotli-dev liblz4-dev libzstd-dev ffmpeg python3
dpkg-buildpackage -us -uc -b -j4
```

For a local (non-package) install:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel 4
sudo cmake --install build --prefix /usr/local
```

See the [development guide](.github/DEVELOPMENT.md) for the full build,
packaging, patching, and CI details.

## Quick start

### USB

1. Connect the Linux device's **device-capable USB port** to the host.
2. Check that the gadget came up:

   ```bash
   systemctl status usbdevice.service
   /usr/bin/usbdevice status
   ```

3. On the host:

   ```bash
   adb devices
   adb shell
   ```

If the port is in host mode, there is no UDC and the service is a harmless
no-op. This is normal.

### TCP or VSOCK

Set adbd's network environment for the service with a systemd drop-in:

```bash
sudo systemctl edit usbdevice.service
```

```ini
[Service]
Environment=ADBD_LISTEN=tcp:127.0.0.1:5555
```

Then restart:

```bash
sudo systemctl restart usbdevice.service
adb connect 127.0.0.1:5555
```

Use `tcp:5555` only on a trusted network, or `vsock:5555` where VSOCK is
available. Loopback is the safe default for TCP.

## Configuration

### USB gadget identity

Edit `/etc/default/adbd-gadget` and restart `usbdevice.service`.

| Variable | Default | Purpose |
| --- | --- | --- |
| `ADBD_GADGET_IDVENDOR` | `0x18d1` | USB vendor ID |
| `ADBD_GADGET_IDPRODUCT` | `0x4ee7` | USB product ID |
| `ADBD_GADGET_NAME` | `adbd` | configfs gadget name |
| `ADBD_GADGET_BCDDEVICE` | `0x0100` | device revision |
| `ADBD_GADGET_MANUFACTURER` | device tree / `Linux Foundation` | USB manufacturer string |
| `ADBD_GADGET_PRODUCT` | `ADB Interface` | USB product string |
| `ADBD_GADGET_SERIAL` | device tree / machine-id | USB serial string |
| `ADBD_GADGET_FFS_UID` | `ADBD_USER`, else user running `usbdevice` | FunctionFS mount owner |
| `ADBD_GADGET_FFS_GID` | `ADBD_USER`, else user running `usbdevice` | FunctionFS mount group |

### Run adbd as a normal user

`ADBD_USER` in `/etc/default/adbd-gadget` selects the account adbd, and
therefore every `adb shell`, runs as. It is empty by default, which keeps adbd
running as root. Set `auto` to pick the first regular account (uid 1000+ with a
valid login shell), or set a name to pick one explicitly. In the non-root mode
`usbdevice` still sets up the gadget as root and gives the user ownership of
FunctionFS. A small root watcher binds the UDC only after adbd has opened both
FunctionFS bulk endpoints, so the non-root daemon never needs configfs write
access; the watcher also re-binds after USB reconnect.

```ini
ADBD_USER=auto
```

Restart `usbdevice.service`. `adb root` no longer works in this mode.

### USB NCM network (disabled by default)

USB networking, including its DHCP/NAT internet sharing, is off by default:
`ADBD_GADGET_NET=none` and `usbnetwork.service` is not enabled. Only adb is
exposed.

The gadget can expose a CDC-NCM Ethernet function in the same USB configuration
as adb. Windows 10/11, Linux and macOS use their in-box drivers; no custom INF
is required:

```bash
sudo sed -i 's/^#\?ADBD_GADGET_NET=.*/ADBD_GADGET_NET=ncm/' /etc/default/adbd-gadget
sudo systemctl enable usbnetwork.service
sudo systemctl restart usbdevice.service
sudo systemctl restart usbnetwork.service
systemctl status usbnetwork.service
ip -4 addr show usb0
```

To disable it again:

```bash
sudo sed -i 's/^#\?ADBD_GADGET_NET=.*/ADBD_GADGET_NET=none/' /etc/default/adbd-gadget
sudo systemctl disable --now usbnetwork.service
sudo systemctl restart usbdevice.service
```

When enabled, `usbnetwork.service` assigns `192.168.42.1/24` to `usb0`, serves
DHCP with dnsmasq, and NATs `192.168.42.0/24` to the board's default uplink.

| Variable | Default | Purpose |
| --- | --- | --- |
| `ADBD_GADGET_NET` | `none` | Add the NCM function; set to `ncm` to enable |
| `ADBD_GADGET_NCM_FUNCTION` | `ncm.usb0` | configfs NCM function name |
| `ADBD_USBNET_IFACE` | `usb0` | NCM network interface |
| `ADBD_USBNET_ADDRESS` | `192.168.42.1` | Board address on the USB network |
| `ADBD_USBNET_PREFIX` | `24` | Address prefix |
| `ADBD_USBNET_NETWORK` | `192.168.42.0/24` | Subnet used by the NAT rule |
| `ADBD_USBNET_DHCP_START` | `192.168.42.100` | First DHCP client address |
| `ADBD_USBNET_DHCP_END` | `192.168.42.200` | Last DHCP client address |
| `ADBD_USBNET_DNS` | `1.1.1.1` | DNS server handed to the host |
| `ADBD_USBNET_UPLINK` | default route | NAT uplink override |

The host gets a DHCP address such as `192.168.42.100`; the board is reachable
at `192.168.42.1`. Internet access also requires a working uplink and
`dnsmasq`/`nftables` on the board.

### SMB share (opt-in)

The package recommends Samba but does not configure a share by default. After
NCM is working, enable it once:

```bash
sudo adbd-smb enable
```

The command adds the `/home/robo` share to `/etc/samba/smb.conf`, prompts for
the SMB password when the user is not yet registered, validates the config
and starts `smbd`. The Windows path is:

```text
\\192.168.42.1\robo
```

```bash
sudo adbd-smb status
sudo adbd-smb disable
```

Settings live in `/etc/default/adbd-smb`; the share is restricted to
`192.168.42.0/24` and loopback. `adbd-smb disable` removes the share and stops
`smbd`.

### adbd runtime

| Variable | Meaning |
| --- | --- |
| `ADBD_LISTEN` | Comma-separated socket specs, e.g. `tcp:127.0.0.1:5555` or `vsock:5555` |
| `ADBD_PORT` | Loopback TCP port when `ADBD_LISTEN` is unset |
| `ADB_TCP_PORT` | Legacy fallback for `ADBD_PORT` |
| `ADBD_SHELL` | Shell executable; defaults to `bash` when available, otherwise `/bin/sh` |
| `ADBD_USB=0` | Disable USB discovery for isolated tests |
| `ADBD_USER` | empty (root) | empty keeps root; `auto` picks the first regular account; a name pins one |

`ADBD_USER` goes in `/etc/default/adbd-gadget`. The other variables reach adbd
through the process environment, so put them in a `systemd` drop-in or export
them before running `usbdevice` manually.

## Companion tools

### scrcpy

With native scrcpy 5.x on the host, the target can serve a Linux/X11 desktop
through the compatibility entry point:

```bash
scrcpy --no-audio
```

On the target, start the display environment:

```bash
sudo scrcpy-display
```

It starts Xvfb (default `:99`, 1280x720x24) and XFCE, writes
`/run/scrcpy-display.env` for `app_process`, and supports `--display`,
`--width`, `--height`, `--depth`, `--no-xfce`, `--status`, `--test` and
`--stop`. Use `scrcpy-display --help` for details.

Useful environment variables:

| Variable | Meaning |
| --- | --- |
| `SCRCPY_COMPAT_DISPLAY` | X11 display, default `:99` |
| `SCRCPY_COMPAT_WIDTH` / `SCRCPY_COMPAT_HEIGHT` | Captured size |
| `SCRCPY_COMPAT_FFMPEG` | ffmpeg executable |
| `SCRCPY_COMPAT_DEVICE_NAME` | Device name shown by scrcpy |
| `SCRCPY_COMPAT_UINPUT_NAME` | uinput device name |

This is a Linux/Xvfb compatibility layer, not a full Android scrcpy server:
audio is disabled, Android clipboard/notifications/camera are not provided, and
non-ASCII text injection is skipped.

### Android-style commands

The package installs familiar command names:

| Command | Purpose |
| --- | --- |
| `screencap` | Screenshot; `-p` writes PNG |
| `screenrecord` | MP4 recording; supports `--size`, `--bit-rate`, `--time-limit` |
| `input` | `text`, `keyevent`, `tap`, `swipe`, `draganddrop`, `motionevent`, `roll`, `keycombination` |
| `getprop` / `setprop` | Read/write compatibility properties |
| `wm` | `wm size` and `wm density` (read-only) |
| `logcat` | Maps to `journalctl` |
| `dumpsys` | `uptime`, `meminfo`, `cpuinfo`, `activity`, `can`, `thermal`, `power`, `battery`, `display`, `adb` |
| `settings` | `get`, `put`, `delete`, `list` backed by JSON |

Examples:

```bash
adb exec-out screencap -p > shot.png
adb shell input tap 100 200
adb shell input text "hello"
adb shell input keyevent KEYCODE_HOME
adb shell screenrecord --time-limit 10 /tmp/demo.mp4
adb shell dumpsys can
adb shell getprop ro.product.model
adb shell wm size
adb logcat -d -t 100
```

`input text` supports ASCII/Latin-1 only; non-ASCII characters are skipped.
`wm` cannot resize Xvfb and returns failure for write operations.

## Troubleshooting

| Symptom | What to check |
| --- | --- |
| `adb devices` is empty | `systemctl status usbdevice.service`; `/usr/bin/usbdevice status`; `dmesg`; host USB cable/port |
| Service starts but no device | Does the port support device mode? Is a UDC present under `/sys/class/udc`? |
| `usbdevice` reports no UDC | The port is in host mode; this is a no-op, not a failure |
| scrcpy opens but has no video | `Xvfb :99`, the desktop session, and `/usr/bin/ffmpeg` on the target |
| scrcpy input is missing | Check `DISPLAY`, `libX11`/`libXtst`, or fall back to `/dev/uinput` |
| Chinese text input is ignored | `input text` only covers ASCII/Latin-1 |
| Service fails to start | `journalctl -u usbdevice.service -b`, `journalctl -u adbd`, and `/tmp/usbdevice-bind.log` |
| `timed out waiting for FunctionFS/configfs bind` | Check that adbd opened `/dev/usb-ffs/adb/ep1` and `ep2`; see `/tmp/adbd.log` and `/tmp/usbdevice-bind.log`. Verify `usbdevice.service` still runs as root so the bind watcher can write configfs. |
| Network access is denied | Add an explicit `ADBD_LISTEN` socket spec; loopback is safer |

## Security notes

- `adbd` has **no authentication** and no TLS pairing. Anyone who can reach the
  endpoint can get a shell as the user running `adbd`.
- Prefer USB or `tcp:127.0.0.1:...`; never expose `tcp:5555` to an untrusted LAN.
- The gadget manager needs root to write configfs and mount FunctionFS.
- `ADBD_USB=0` is useful for tests that must not touch USB.

## Development

Development documentation lives in [`.github/DEVELOPMENT.md`](.github/DEVELOPMENT.md)
(Chinese: [`.github/DEVELOPMENT_CN.md`](.github/DEVELOPMENT_CN.md)). It covers:

- source layout and pinned upstream revisions
- building, packaging, and applying the Linux portability patch
- USB gadget and scrcpy protocol internals
- test commands, CI, and release process
- portability boundaries and validation status

## License

The Linux port, packaging, and companion tools are licensed under **GPL-3.0**.
Upstream AOSP components, fmt, and the adapted patch retain their original
licenses. See `debian/copyright` and `LICENSE`.
