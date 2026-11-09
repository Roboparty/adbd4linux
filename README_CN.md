<div align="center">

# Linux adbd

**在 Debian/Ubuntu Linux 上运行的独立、无 Android framework 的 ADB 守护进程。**

[English](README.md) · [中文](README_CN.md) · [开发文档](.github/DEVELOPMENT_CN.md)

</div>

> **一句话说明：** 这个包让普通 Linux 主机通过 USB（或 TCP/VSOCK）暴露一个真正的
> `adbd` 端点，主机端标准 `adb` 和 `scrcpy` 可以直接连接，不需要 Android
> framework、SurfaceFlinger、MediaCodec，也不需要完整 Android 编译环境。

## 为什么用它？

- **真正的 `adbd`，不是模拟器。** 编译 AOSP 设备端 `adbd`，安装为 `/usr/bin/adbd`。
- **USB gadget 开箱即用。** 自带通用 FunctionFS 管理器：准备 configfs、挂载
  FunctionFS、启动 `adbd`，并绑定第一个可用 UDC。
- **需要时可走网络。** 支持显式 TCP 和 VSOCK 监听；默认不开放任何网络端口。
- **配套工具齐全。** 可选 scrcpy 和 Android 风格命令，让 Linux 桌面更像 Android 目标。
- **不需要 Android 平台构建。** 固定一组上游源码，加一个 Linux 可移植补丁即可。

## 包里有什么？

| 组件 | 作用 |
| --- | --- |
| `adbd` | 设备端 ADB 守护进程：shell、文件同步、端口转发和传输处理 |
| `usbdevice` | USB FunctionFS gadget 管理器和 systemd 服务 |
| `usbnetwork` | adb gadget 的可选 NCM DHCP/NAT 服务 |
| `adbd-smb` | 可选的 Samba 共享助手；默认不启用 |
| `app_process` + `scrcpy-compat` | 原生 scrcpy 5.x 使用的入口和服务端 |
| Android 命令兼容层 | `screencap`、`screenrecord`、`input`、`getprop`、`setprop`、`wm`、`logcat`、`dumpsys`、`settings` |

## 环境要求

- 支持 **configfs** 和 **FunctionFS** 的 Linux。
- 能作为 **device** 工作的 USB 端口/控制器（UDC），或可用的 TCP/VSOCK。
- Debian/Ubuntu 包管理和 **systemd**。
- 主机侧：`adb`；可选 `scrcpy`。
- 目标侧使用 scrcpy：`Xvfb`、`ffmpeg` 和 X11 桌面/会话；
  使用 `scrcpy-display` 启动（见下文）。
- 目标侧使用 Android 风格命令：Python 3、`ffmpeg`、`x11-utils`、`libx11-6`、
  `libxtst6`、`iproute2`、`procps`。

> **安全提醒：** `adbd` 是无认证调试接口。只放在可信 USB、回环地址或私有网络中。
> 不要把 `tcp:5555` 暴露到不可信网络。

## 安装

### Debian/Ubuntu 安装包

```bash
sudo apt install ./adbd_*.deb
```

如果包管理器需要修复依赖：

```bash
sudo dpkg -i adbd_*.deb
sudo apt-get -f install
```

安装包会安装 `usbdevice.service`，并在 `sysinit.target` 下启用。

### 从源码构建

```bash
git clone --recurse-submodules <repository-url>
cd adbd
sudo apt-get install build-essential debhelper cmake pkg-config patch \
    libbrotli-dev liblz4-dev libzstd-dev ffmpeg python3
dpkg-buildpackage -us -uc -b -j4
```

本地（非打包）安装：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel 4
sudo cmake --install build --prefix /usr/local
```

完整构建、打包、补丁和 CI 说明见[开发文档](.github/DEVELOPMENT_CN.md)。

## 快速开始

### USB

1. 把 Linux 设备的**支持 device 模式的 USB 口**接到主机。
2. 检查 gadget 是否启动：

   ```bash
   systemctl status usbdevice.service
   /usr/bin/usbdevice status
   ```

3. 在主机上执行：

   ```bash
   adb devices
   adb shell
   ```

如果 USB 口处于 host 模式，就没有 UDC，服务会安全地什么都不做。这是正常现象。

### TCP 或 VSOCK

通过 systemd drop-in 给服务设置 adbd 的网络环境：

```bash
sudo systemctl edit usbdevice.service
```

```ini
[Service]
Environment=ADBD_LISTEN=tcp:127.0.0.1:5555
```

然后重启：

```bash
sudo systemctl restart usbdevice.service
adb connect 127.0.0.1:5555
```

只在可信网络使用 `tcp:5555`，或者在支持 VSOCK 的环境中使用 `vsock:5555`。
TCP 默认建议只监听回环地址。

## 配置

### USB gadget 身份

编辑 `/etc/default/adbd-gadget` 后重启 `usbdevice.service`。

| 变量 | 默认值 | 作用 |
| --- | --- | --- |
| `ADBD_GADGET_IDVENDOR` | `0x18d1` | USB vendor ID |
| `ADBD_GADGET_IDPRODUCT` | `0x4ee7` | USB product ID |
| `ADBD_GADGET_NAME` | `adbd` | configfs gadget 名称 |
| `ADBD_GADGET_BCDDEVICE` | `0x0100` | 设备版本 |
| `ADBD_GADGET_MANUFACTURER` | 设备树 / `Linux Foundation` | USB 厂商字符串 |
| `ADBD_GADGET_PRODUCT` | `ADB Interface` | USB 产品字符串 |
| `ADBD_GADGET_SERIAL` | 设备树 / machine-id | USB 序列号 |
| `ADBD_GADGET_FFS_UID` | `ADBD_USER`，否则为运行 `usbdevice` 的用户 | FunctionFS 挂载属主 |
| `ADBD_GADGET_FFS_GID` | `ADBD_USER`，否则为运行 `usbdevice` 的用户组 | FunctionFS 挂载属组 |

### 以普通用户运行 adbd

`/etc/default/adbd-gadget` 的 `ADBD_USER` 决定 adbd 及所有 `adb shell` 以
哪个账号运行。默认留空，保持 adbd 以 root 运行；设为 `auto` 取第一个普通账号
（uid 1000+ 且 shell 可登录）；也可以写具体用户名。非 root 模式下
`usbdevice` 仍以 root 配置 gadget，并把 FunctionFS 的属主交给该用户。

```ini
ADBD_USER=auto
```

重启 `usbdevice.service`。该模式下 `adb root` 不再可用。

### USB NCM 网络（默认禁用）

USB 网络（含 DHCP/NAT 供网）默认关闭：`ADBD_GADGET_NET=none`，且
`usbnetwork.service` 不自启动，只保留 adb 功能。

需要时可以让 gadget 在同一个 USB configuration 里同时提供 adb 和 CDC-NCM
网卡。Windows 10/11、Linux 和 macOS 都用系统自带驱动，不需要额外 INF：

```bash
sudo sed -i 's/^#\?ADBD_GADGET_NET=.*/ADBD_GADGET_NET=ncm/' /etc/default/adbd-gadget
sudo systemctl enable usbnetwork.service
sudo systemctl restart usbdevice.service
sudo systemctl restart usbnetwork.service
systemctl status usbnetwork.service
ip -4 addr show usb0
```

重新禁用：

```bash
sudo sed -i 's/^#\?ADBD_GADGET_NET=.*/ADBD_GADGET_NET=none/' /etc/default/adbd-gadget
sudo systemctl disable --now usbnetwork.service
sudo systemctl restart usbdevice.service
```

启用后 `usbnetwork.service` 会：

- 给 `usb0` 配 `192.168.42.1/24`
- 用 dnsmasq 发 DHCP
- 把 `192.168.42.0/24` NAT 到板子的默认上行

| 变量 | 默认值 | 作用 |
| --- | --- | --- |
| `ADBD_GADGET_NET` | `none` | 是否加入 NCM 功能，设为 `ncm` 启用 |
| `ADBD_GADGET_NCM_FUNCTION` | `ncm.usb0` | configfs NCM 功能名 |
| `ADBD_USBNET_IFACE` | `usb0` | NCM 网卡名 |
| `ADBD_USBNET_ADDRESS` | `192.168.42.1` | 板端 USB 网络地址 |
| `ADBD_USBNET_PREFIX` | `24` | 前缀长度 |
| `ADBD_USBNET_NETWORK` | `192.168.42.0/24` | NAT 使用的网段 |
| `ADBD_USBNET_DHCP_START` | `192.168.42.100` | DHCP 起始地址 |
| `ADBD_USBNET_DHCP_END` | `192.168.42.200` | DHCP 结束地址 |
| `ADBD_USBNET_DNS` | `1.1.1.1` | 发给主机的 DNS |
| `ADBD_USBNET_UPLINK` | 默认路由 | 手动指定 NAT 上行 |

主机一般会拿到 `192.168.42.100` 之类的地址，板子地址是 `192.168.42.1`。
要上网还需要板子本身有可用上行，并安装 `dnsmasq`/`nftables`。

### SMB 共享（可选）

包会推荐安装 Samba，但默认不配置、不启用共享。NCM 正常后执行一次：

```bash
sudo adbd-smb enable
```

命令会：

- 把 `/home/robo` 共享追加到 `/etc/samba/smb.conf`
- 用户未注册时提示设置 SMB 密码
- 校验配置并启动 `smbd`

Windows 访问路径：

```text
\\192.168.42.1\robo
```

```bash
sudo adbd-smb status
sudo adbd-smb disable
```

配置在 `/etc/default/adbd-smb`；共享默认只允许 `192.168.42.0/24` 和
loopback。`adbd-smb disable` 会移除共享并停止 `smbd`。

### adbd 运行环境

| 变量 | 含义 |
| --- | --- |
| `ADBD_LISTEN` | 逗号分隔的监听地址，例如 `tcp:127.0.0.1:5555`、`vsock:5555` |
| `ADBD_PORT` | 未设置 `ADBD_LISTEN` 时的回环 TCP 端口 |
| `ADB_TCP_PORT` | `ADBD_PORT` 的旧版兼容变量 |
| `ADBD_SHELL` | shell 路径；优先 `bash`，没有时用 `/bin/sh` |
| `ADBD_USB=0` | 禁用 USB 探测，供隔离测试使用 |
| `ADBD_USER` | 空（root） | 留空保持 root；`auto` 取第一个普通账号；写用户名则固定 |

`ADBD_USER` 放在 `/etc/default/adbd-gadget`。其余变量需要进入 adbd 的进程
环境，所以放在 systemd drop-in 中，或手动运行 `usbdevice` 前 export。

## 配套工具

### scrcpy

主机使用原生 scrcpy 5.x，目标通过兼容入口提供 Linux/X11 桌面：

```bash
scrcpy --no-audio
```

目标侧先启动显示环境：

```bash
sudo scrcpy-display
```

脚本会启动 Xvfb（默认 `:99`、1280x720x24）和 XFCE，写入
`/run/scrcpy-display.env` 供 `app_process` 使用；支持 `--display`、
`--width`、`--height`、`--depth`、`--no-xfce`、`--status`、`--test`、
`--stop`。详细用法见 `scrcpy-display --help`。

常用环境变量：

| 变量 | 含义 |
| --- | --- |
| `SCRCPY_COMPAT_DISPLAY` | X11 display，默认 `:99` |
| `SCRCPY_COMPAT_WIDTH` / `SCRCPY_COMPAT_HEIGHT` | 采集尺寸 |
| `SCRCPY_COMPAT_FFMPEG` | ffmpeg 可执行文件 |
| `SCRCPY_COMPAT_DEVICE_NAME` | scrcpy 显示的设备名 |
| `SCRCPY_COMPAT_UINPUT_NAME` | uinput 设备名 |

这是 Linux/Xvfb 兼容层，不是完整 Android scrcpy server：音频关闭，
不支持 Android 剪贴板/通知/摄像头，非 ASCII 文本注入会跳过。

### Android 风格命令

安装包提供常用命令名：

| 命令 | 作用 |
| --- | --- |
| `screencap` | 截屏；`-p` 输出 PNG |
| `screenrecord` | 录屏 MP4，支持 `--size`、`--bit-rate`、`--time-limit` |
| `input` | `text`、`keyevent`、`tap`、`swipe`、`draganddrop`、`motionevent`、`roll`、`keycombination` |
| `getprop` / `setprop` | 读取/写入兼容属性 |
| `wm` | `wm size` 和 `wm density`（只读） |
| `logcat` | 映射到 `journalctl` |
| `dumpsys` | `uptime`、`meminfo`、`cpuinfo`、`activity`、`can`、`thermal`、`power`、`battery`、`display`、`adb` |
| `settings` | `get`、`put`、`delete`、`list`，JSON 存储 |

示例：

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

`input text` 只支持 ASCII/Latin-1，非 ASCII 字符会跳过。
`wm` 不能修改 Xvfb 尺寸，写操作会返回失败。

## 排查

| 现象 | 检查项 |
| --- | --- |
| `adb devices` 为空 | `systemctl status usbdevice.service`；`/usr/bin/usbdevice status`；`dmesg`；主机 USB 线和端口 |
| 服务启动但没有设备 | USB 口是否支持 device 模式？`/sys/class/udc` 下是否有 UDC？ |
| `usbdevice` 提示没有 UDC | USB 口处于 host 模式；这是 no-op，不是故障 |
| scrcpy 能打开但没有画面 | 目标上的 `Xvfb :99`、桌面会话和 `/usr/bin/ffmpeg` |
| scrcpy 没有输入 | 检查 `DISPLAY`、`libX11`/`libXtst`，或确认是否使用 `/dev/uinput` 回退 |
| 中文输入无效 | `input text` 只支持 ASCII/Latin-1 |
| 服务启动失败 | `journalctl -u usbdevice.service -b` 和 `journalctl -u adbd` |
| 网络连接被拒绝 | 显式设置 `ADBD_LISTEN`；回环地址更安全 |

## 安全说明

- `adbd` **没有认证**，也没有 TLS 配对。能连到端点的人就能以运行 `adbd` 的用户身份拿到 shell。
- 优先使用 USB 或 `tcp:127.0.0.1:...`；不要把 `tcp:5555` 暴露到不可信局域网。
- gadget 管理器需要 root 才能写 configfs 和挂载 FunctionFS。
- 测试时可用 `ADBD_USB=0` 完全避免触碰 USB。

## 开发

开发文档位于 [`.github/DEVELOPMENT_CN.md`](.github/DEVELOPMENT_CN.md)
（英文版：[`.github/DEVELOPMENT.md`](.github/DEVELOPMENT.md)）。内容包括：

- 源码结构和固定的上游版本
- 构建、打包和 Linux 可移植补丁的应用方式
- USB gadget 与 scrcpy 协议内部细节
- 测试命令、CI 和发布流程
- 可移植性边界和验证状态

## 许可证

Linux 移植、打包和配套工具使用 **GPL-3.0**。上游 AOSP 组件、fmt 和适配补丁
保留各自原许可证。详见 `debian/copyright` 和 `LICENSE`。
