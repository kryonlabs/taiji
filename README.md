# TaijiOS

TaijiOS is a bootable desktop OS tree based on the 9legacy branch of
Plan 9 from Bell Labs. See [README](README) for the historical upstream
README file.

To boot TaijiOS, install qemu, so that you have `qemu-system-x86_64` in your path.
Then:

	git clone https://github.com/rsc/plan9 taiji
	./taiji/boot/qemu

The qemu script builds u9fs in taiji/sys/src/cmd/unix/u9fs and then runs
qemu with the right options to boot diskless, using the git clone as the
root file system.

Because the VM shares the files with your host machine, you can edit files in one place
and see the changes instantly in the other place. For example, you can edit files in your
local editor even if you are running tests in the TaijiOS VM.
You can run builds of Go binaries targeting TaijiOS on your host machine
and then test the binaries in the VM.
And you can run more than one VM, all sharing the same file system.

At boot time, the startup disk boot/pxeboot.raw loads a minimal TaijiOS kernel
into memory, which then PXE loads a plan9.ini and new kernel over TFTP (provided by qemu).
So if you make changes to the kernel, you can boot from `ether0!/sys/src/9/pc/9pc`
to test an as-yet-uninstalled kernel.

The plan9.ini is loaded from [/cfg/pxe/525400123456](cfg/pxe/525400123456).
(That number is the VM's MAC address.)
Changes made to that file will be visible on the next VM boot.
