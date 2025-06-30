# QEMU

There is no native 3D acceleration support for QEMU yet, but you can apply QEMU-3dfx patches.

Next problem with QEMU is, that Windows 98 incorrectly detected PCI bus as PnP BIOS. There is 2 solutions for it.

#### Non-PnP BIOS

This is best for fresh installations. First you need [SeaBIOS](https://www.seabios.org/SeaBIOS) with disabled `CONFIG_PNPBIOS`. You can compile manually from source or you can use my binary: [seabios-qemu.zip](https://files.emulace.cz/seabios-qemu.zip). Extract `bios.bin` somewhere and run QEMU with '-bios /path/to/somewhere/bios.bin'. Windows 9x installation with this BIOS should detect all hardware without problems.


#### PCI bus detection fix

If you have already installed system and you don't see any PCI hardware, use these steps:

1) Open Device Manager and locate *Plug and Play BIOS* (Exclamation mark should be on it)

![QEMU PCI: Plug and Play BIOS](resource/docs/qemu-pci-1.png)

2) With this device selected click on *Properties*, select *Driver* tab and click on *Update driver*

![QEMU PCI: Plug and Play BIOS properties](resource/docs/qemu-pci-2.png)

3) In Wizard select second option (*Display a list of all drivers in specific location, ...*)

![QEMU PCI: list drivers](resource/docs/qemu-pci-3.png)

4) Select *Show all hadrware* and from models list choose **PCI bus**, click on *next*, confirm 
warning message and reboot computer.

![QEMU PCI: select PCI bus](resource/docs/qemu-pci-4.png)
![QEMU PCI: warning message](resource/docs/qemu-pci-5.png)

5) After reboot, system will ask you for drive on every new discovered device. All you need to do, is select *Search for the best driver...* and clicking on *next*. Please don't select new or updated drivers here - you can do it later. You may need reboot computer several times.

![QEMU PCI: new PCI device](resource/docs/qemu-pci-6.png)
![QEMU PCI: select best driver](resource/docs/qemu-pci-7.png)

6) You will be asked for VGA driver and unknown device drivers. Still select default generic driver here!

![QEMU PCI: standard PCI VGA](resource/docs/qemu-pci-8.png)
![QEMU PCI: unknown device](resource/docs/qemu-pci-9.png)

7) After last reboot open *Device manager* again - as you see, you have 2 VGA cards now, so select *Standard Display Adapter (VGA)* (the working one) and click on *Remove*.

![QEMU PCI: 2 VGA adapters](resource/docs/qemu-vga.png)

8) After reboot (again), you have working system now and you can install SoftGPU and other drivers.

# QEMU-3dfx

1) [Built patched QEMU](https://github.com/kjliew/qemu-3dfx?tab=readme-ov-file#building-qemu)
2) Install Windows 98 with disabled CPU accelerator (it's a bit slow)
3) Check if you see PCI bus on Hardware manager
4) (optional) Install audio driver you're using AC-97
5) Mount SoftGPU ISO and install SoftGPU
6) Reboot and check if video driver works
7) Now you can shutdown VM and run again with CPU accelerator enabled
8) Now navigate to SoftGPU CD to `extras\qemu3dfx` folder and you have do set the signature:

For QEMU-3dfx need both wrapper and hypervisor same signature to works. This signature is first 7 characters from GIT revision hash. You can obtain the hash by this command in cloned qemu-3dfx repository:
```
git rev-parse HEAD
```
Binaries in SoftGPU allows to override build signature registry keys. To check that you have same signature as QEMU run `testqmfx.exe` (in `extras\qemu3dfx`). If you see error 0x45A (= ERROR_DLL_INIT_FAILED), you have wrong signature. In this case edit `set-sign.reg` (copy it from CD to writeable location) and rewrite the value `REV_QEMU3DFX` to revision hash obtain from GIT (you need only first 7 characters, retype full hash isn't necessary). After it apply file to registry (by double click on file) and run `testqmfx.exe` to check the result - you should see rotating triangle on success and see OpenGL information from your host GPU.

9) Copy `fxmemmap.vxd` and `qmfxgl32.dll` to `C:\WINDOWS\SYSTEM` and apply file `icd-enable.reg` (this tells to driver using `qmfxgl32.dll` when system `opengl32.dll` ask about OpenGL driver).
10) reboot (**required**)
11) run *GLchecker* or some other 3D application to verify settings.
