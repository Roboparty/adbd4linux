# Linux USB reconnect regression

Build normally with CMake or `dpkg-buildpackage`. CTest runs
`linux_ffs_binding_test`, including repeated empty-to-bound transitions,
idempotency, conflicting controllers, missing configfs, and invalid settings.

`usbdevice` supplies `ADBD_GADGET_UDC_PATH` and `ADBD_GADGET_UDC` to adbd.
After opening FunctionFS endpoints, adbd binds that gadget only. An adbd started
without these variables leaves binding to its external gadget manager.
The start script creates function links before launching adbd and waits for its
binding; stop waits for adbd to exit before dismantling configfs.

Hardware acceptance requires an independent SSH connection. Record boot ID,
adbd PID, UDC binding and host `adb devices -l`. Unplug and reconnect USB several
times, including a disconnect longer than ten seconds. After each round, require
the same boot ID and adbd PID, a configured UDC, a usable adb shell and a TUI RPC
response. Also check that intentional `usbdevice stop` leaves the gadget stopped,
and `usbdevice start` restores it. Software UDC unbind/rebind is a useful separate
regression test but does not substitute for physical unplug/replug.
