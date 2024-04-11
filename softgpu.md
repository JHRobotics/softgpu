# SoftGPU
SoftGPU is pack of driver, wrappers, and system components to make 3D acceleration work on Microsoft Windows 9x Systems (Windows 95, Windows 98, Windows Me) in a Virtual Machine.

## Requirements

### Hypervisor
- For maximum HW 3D feature support:
	- Oracle VirtualBox 7.0 
- HW 3D acceleration without pixel shader support:
	- Oracle VirtualBox 6.1
	- VMWare Workstation 16.x - 17.x
	- VMWare Player 16.x - 17.x
- 3D pass-through
	- QEMU 6.x - 8.x + [QEMU 3Dfx](https://github.com/kjliew/qemu-3dfx)
- SW 3D acceleration
	- QEMU 6.x - 8.x
	- VirtualBox 5.1, 6.0

### Hardware
- Intel Core2 CPU or AMD's equivalent
- VM-x/AMD-V CPU instruction support and enabled in BIOS/firmware
- 4 GB RAM
- GPU for HW acceleration:
	- Oracle VirtualBox 7.0: DirectX 11.1 or Vulcan compatible
	- VMWare Workstation/Player: DirectX 9.0c with Shader Model 3 and OpenGL 2.1
	- Oracle VirtualBox 6.1: DirectX 9.0c on Windows host, OpenGL 2.1 on Linux

### Virtual Machine
- CPU acceleration
- Pentium II instruction set (when CPU is emulated)
- 1 GB free HDD space
- 512 MB RAM
- Microsoft Windows 95, 98, 98 SE or Me (Windows 98 SE is recommended)
- [patcher9x](https://github.com/JHRobotics/patcher9x/releases)
- [R. Loew's patchmem](http://files.emulace.cz/patchmem.zip) (if you want to set more RAM than 512 MB)
- OpenGL95 (included, for Windows 95 without OpenGL)
- MSVCRT (included, for Windows 95)
- DirectX Redistributable 8 or 9 (included)

## Virtual GPU

Historically video card in Virtual Machines were designed for only rendering framebuffer - they're simply to implement, both on hypervisor side and on BIOS/firmware in VM, drivers for OS in VM are simply too and on most cases there not needed and most modern OS can use to some basic drawing using (video) BIOS calls. This is logical, because VM are usually used as servers, and you don't want to waste resources on something that is not visible. This is important to know because (by my opinion) most problem with graphic on virtual desktop is that these technologies were developed for server use and has been only a bit fixed for desktop usage.

Following are the most common types of virtual video adapters:

### Bochs VBE Extensions (aka VBoxVGA or -vga std)

On this API is based basic video adapter in VirtualBox (VBoxVGA) and on QEMU (`-vga std`). There is no 2D or 3D acceleration support in this video card. SoftGPU support VirtualBox and QEMU variant but all drawing is done by software. It is slow but is most compatible and works in all cases. Key feature for speed of SW rendering is SSE instructions (at least SSE 3) support on VM and virtual OS (>= Windows 98).

### VMware SVGA3D (aka VBoxSVGA or VMSVGA)

This is real (virtual) GPU and SoftGPU can use this for accelerated rendering. This GPU can occur in multiple variant.

#### SVGA-II (aka -vga vmware)

Simplest variant supported event in QEMU but with very limited 2D acceleration. This variant still using full software rendering with similar results as VBE. To this variant is GPU degraded if is 3D acceleration turned off in VMware or VirtualBox.

QEMU isn't support 8 and 16 bit modes, SoftGPU can work on it, but most games won't start. Please use `-vga std` in QEMU instead.

#### SVGA3D gen.9 (vGPU9)

Supported by VMware and VirtualBox 6.1 and 7.0, mainly designed for accelerate DX 9 drawing (usually for Windows Vista and 7 aero). Limiting is shader model used in this GPU and isn't possible to use pixel/vertex shaders in DX8 and DX9 games. Again, isn't possible use Mesa Nine (DX9 translation to native driver) with this GPU variant.

#### SVGA3D gen.10 (vGPU10)

Supported only in VirtualBox 7.0. (On VMware is theoretically supported too, but I'm currently not able to turn on it for Windows 9x guest). This is designed for Windows 10 for DirectX 11/12 support - DirectX 12 can emulate all older DX variant and native DDI isn't needed (but this works only on W7 and W10/W11).

DX8/DX9/OpenGL Shader are supported, and most DX 9 games works, OpenGL rendering in games is faster than with vGPU9. This is actually most preferred way to use with SoftGPU. Disadvantage of using vGPU10 is that all textures are stored in VM RAM so you need more RAM to attach to VM. 512 MB RAM is minimum recommended value for vGPU10 and for Windows 9x is value also maximum recommended without additional patches.

#### SVGA-III

Newest variant primary developed for ARM devices, currently not supported by SoftGPU. (But when this will be supported on VirtualBox or another opensource HV, I will include support of this to SoftGPU too).


### VirGL

This virtual GPU for QEMU, potentially faster than SVGA3D. Not supported by SoftGPU yet.

### Real GPU (PCI passthrough)

This is connection of real GPU to virtual machine. Works (as I known) only on QEMU in Linux (as Host). Need extra GPU compatible with Windows 9x - I personally had relative success with PCI express variant of nvidia 6600/6600 GT. You also need specific HW (free PCI express slot, VT-d CPU support, mother board support for PCIe slot isolation and second monitor + keyboard and mouse). I also observe very massive performance loss. I recommend this only for most advanced users. SoftGPU is no needed there because you can use native GPU drivers.

### Virtual GPU (PCI passthrough)

Some GPUs allow to split the real GPU to multiple virtual GPUs and attach them to virtual machines. This feature isn't commonly present in gaming hardware (for example for nvidia you need quadro card), only common technology is Intel GVT-g. But there are no drivers for Windows 9x guest. I see too many problems to consider support in SoftGPU.

### Emulated GPU

This is domain for emulators. Software like DOSBox (DOSBox-X) and PCem allow emulate real GPU like S3 Trio or S3 ViRGE and 3D accelerator like Voodoo/Voodoo2. This allows to use unchanged legacy software and driver. SoftGPU support is not needed here.


## 3D passthrough

This technique creates side channel so 3D commands do not go to virtual GPU, but they are serialized and send to host machine where are unpacked and send to real GPU. This is much simpler than create full-weight virtual GPU. Can be do it over shared memory or via network. Disadvantages are problems with transporting rendered image back to VM (for example to draw in Window or postprocess) and with multiple contexts (more different application want render own graphics same time). There are some solutions:

### QEMU-3Dfx

More on [QEMU-3Dfx page](https://github.com/kjliew/qemu-3dfx). SoftGPU is supporting this. See instructions on [GitHub main page](https://github.com/JHRobotics/softgpu/#qemu-3dfx-usage) how to use.

### Chromium

This is [very old software](https://chromium.sourceforge.net/), but it's modification was used in VirtualBox to version 5.1 to accelerate 3D rendering. Currently is abandoned and not supported by SoftGPU.


## Virtual Machine configuration

General tips how to configure individual software is on [Github project main page](https://github.com/JHRobotics/softgpu/).


## Installation

Mount SoftGPU CD or unpack zip package to VM HDD and run softgpu.exe, revise options (default are recommended for most users and configurations) and click on **Install** button.

Options descriptions follow:

### Binaries types

SoftGPU support 2 differed CPU architectures:

**Pentium II** is mainly for Windows 95 natively which supports only MMX instruction set (and event with SSE hack is required to minimize usage of SSE/AVX instructions to only one application) or when your virtual CPU is emulated and advanced instruction isn't available.

**SSE 3** is for Windows 95/98 and is much faster and because I want to keep compatibility with old Intel Core2 architecture, is SSE instruction set limited to SSE3.

**SSE 4.2** using maximum CPU extensions which Windows 98/Me supporting. Speed is slightly faster than **SSE3** but depends on CPU. There binaries aren't included in standard releases, I'm including them to edition for supporters as gift for their support. But in every case you can build them yourself by add this `CFLAGS` to `config.mk`:

```TUNE += -march=westmere```


### Wrappers

Currently GPU driver natively support only **DirectDraw** and **OpenGL** and other API needs to translated to them.

#### Wine D3D (Wine9x)

Translating DirectDraw and DirectX to OpenGL. Although Wine has many bugs and graphical glitches it is still preferred way to emulate DirectX.

#### Mesa Nine (Mesa9x)

Translating DirectX 9 directly to driver primitives (if is based on Mesa3D library) and in theory should be faster than Wine. But require higher shader model so works only for vGPU10. Also Wine solving some quirks in Windows software so is more compatible and stable than Nine.

#### Eight-to-Nine (Mesa9x)

Since Nine supporting only, DX9 there is wrapper to translate DX8 to DX9. It is based on [d3d8to9](https://github.com/crosire/d3d8to9) project. All is statically linked to one DLL, so you don't need to worry about DLL ping-pong. Is is slightly faster than Wine.


#### OpenGlide (OpenGlide9x)

This wrapper is translating 3Dfx Glide to OpenGL. Currently is supported only Glide2x and Glide3x. This wrapper isn't ideal but I making slowly progress. Glide for DOS is not supported and support is not planed. You can use DOS Glide with [QEMU-3Dfx](https://github.com/kjliew/qemu-3dfx), but binaries no included in SoftGPU package.

#### MiniGL

This wrapper was designed to translate OpenGL to (3DFX proprietary) Glide2 or Glide3 API. Usually used for Quake 2/3 engine based games. Since Voodoo 3 was abandoned to native OpenGL implementation. OpenGlide9x now supports these wrappers results OpenGL → Glide → OpenGL transport. But why to do this? There is 2 situations: 1st is fail-safe, so incorrect installation/setup (wrappers were parts of some distributions) can work somehow. 2nd is for games that will not work correctly with Mesa3D OpenGL implementation.

**Usage**

You need some original files:
- Voodoo2 driver (V3.02.02), from [here](http://falconfly.3dfx.pl/minigl.htm) or [my mirror](https://files.emulace.cz/voodoo2-30202.zip)
- 3dfx MiniGL (V1.49, from [here](http://falconfly.3dfx.pl/minigl.htm) or [my mirror](https://files.emulace.cz/minigl149.zip)

From Voodoo2 driver copy `3dfxVGL.dll`, `3dfxSpl2.dll` and `3dfxSpl3.dll` to *WINDOWS/SYSTEM* folder. For games: (GL)Quake, Quake 2, Half Life, Heretic 2, Hexen 2 and Sin copy individual **.dll* from MiniGL archive to game directory (usually there is file with same name, overwrite it). I'm currently recommending to use MiniGL for these games:

- Soldier of Fortune
- Quake 2 (with vGPU10)
- Sin (with vGPU10)
- Heretic II (with vGPU10)

For Quake 3 and RTCW is OpenGlide disabled, because it causes more problems than benefits.
 

### VRAM and GMR

Every Windows 9x application has this memory (RING-3) layout:
- 0x00000000 - 0x7FFFFFFF: private memory (2 GB)
- 0x80000000 - 0xBFFFFFFF: shared memory (1 GB)
- 0xC0000000 - 0xFFFFFFFF: system memory (1 GB)

First 2 GB is private to every application but shared and system are the same for every application (RING-3) or driver (PM16 RING-3 or PM32 RING-0). And here is problem: devices memory is mapped to system. As memory we're speaking about ROM (including BIOS), RAM on devices (VRAM, disk controller cache etc., without system RAM) and I/O space (device HW registers, etc.). If we weren't counting VRAM, on most PCs are only about 300-400 MB free in system memory. This is one of problem, why GPU with 512 MB VRAM, not working on Windows 9x, VRAM just not fit here.

In real VRAM are stored framebuffer, surfaces, textures, and buffers (on modern GPU also graphical primitives, shaders and GPU computation data and code). But on Virtual GPU is stored only framebuffer, VMHAL can use VRAM to store surfaces, but for 1920x1200 is only about 16 MB used, 32 MB for theoretical application using DirectDraw with double buffering. Limiting VRAM can be useful to save system memory space.

But where the textures and rendering buffers stored? In something called Guest Memory Region, this is guest system RAM mapped to virtual GPU. vGPU9 stores in GMR working buffer + system memory mapped textures and GPU only mapped textures are in the host memory (in theory they can be in host VRAM only, but usually they are occupying both host RAM and VRAM). vGPU10 also stores GPU mapped textures in guest system RAM (and this RAM is mapped to real GPU, more or less effectively). So minimal application eats 64 MB memory (~32 MB runtime + 32 MB frame buffer), typical game eats about 128 MB for vGPU9 and 256 for vGPU10. Compressed textures are eating more space, then uncompressed, because you need store both compressed and unpacked variant. 16-bit frame buffer is larger than 32-bit, because all rendering operations are 32-bit and final version it's software blit to 16-bit.

Some later games and 3DMark can eat all 512 MB of RAM. So that why is here GMR limitation, because if the driver (GMR allocation is done by RING-0 driver) eats all usable RAM, system freeze.

SoftGPU will limit VRAM to 128 MB and GMR to 160 MB for 512 MB system and for 256 MB if you have 768 MB RAM and more. These limits can be adjust by this registry keys:

```
REGEDIT4


[HKEY_LOCAL_MACHINE\Software\VMWSVGA]
"VRAMLimit"=dword:00000080

[HKEY_LOCAL_MACHINE\Software\Mesa3D\global]
"SVGA_GMR_LIMIT"="128"
```

GMR limit can be configured per application, 128 MB is usually enough, but 3Dmark03 for example need about 192 MB to work correctly.


## Registry configuration

Where are example how registry configuration work, you can save there example as text file with REG extension and apply them on double click or open `regedit.exe` edit these value directly (create these key when missing).

### Hypervisors bugs override

```
REGEDIT4


[HKEY_LOCAL_MACHINE\Software\VMWSVGA]
;
; For 565 format were reported bad attributes
; You can disable it if you have:
;  VirtualBox >= 6.1.46
;  VirtualBox >= 7.0.10
"RGB565bug"=dword:00000001
;
; Vmware sometime freeze when Command Buffers are used.
; This using older way to send commands to GPU.
; You turn off for VirtualBox, bud vGPU9 is slightly waster
; when using FIFO. This option have no effect to vGPU10
"PreferFIFO"=dword:00000001

[HKEY_LOCAL_MACHINE\Software\Mesa3D\global]
;
;
"SVGA_CLEAR_DX_FLAGS"="1"
```


### Universal driver configuration
```
REGEDIT4


[HKEY_LOCAL_MACHINE\Software\vmdisp9x]
;
; set 1 to use software OpenGL event there is some accelerated
"FORCE_SOFTWARE"=dword:00000000
;
; set 1 when you has working QEMU-3Dfx wrapper
"FORCE_QEMU3DFX"=dword:00000000


```

### SVGA driver configuration

```
REGEDIT4


[HKEY_LOCAL_MACHINE\Software\VMWSVGA]
;
; Limit mapping of VRAM in MB. For Windows 9x is limit
; around 300 MB and if you attach mode, system will crash.
; But keep on might that VRAM is almost unutilized and if you
; don't need 8k resolution isn't reason why increase this value.
"VRAMLimit"=dword:00000080
;
; set 0 to disable command buffer support, use FIFO instead.
; For debug only.
"CommandBuffers"=dword:00000001

```


### DirectX/DirectDraw wrapper selection

Some application won't work or not working well with Wine, for 2D application you can use native DirectDraw HAL. Media player is one of them:

```
REGEDIT4


[HKEY_LOCAL_MACHINE\Software\DDSwitcher]
"MPLAYER2.exe"="system"

```

Some 2D games have these problems to, for example The Sims:

```
REGEDIT4


[HKEY_LOCAL_MACHINE\Software\DDSwitcher]
"sims.exe"="system"

```

You can also select native DirectDraw HAL by default, and turn on wine for individual applications:

```
REGEDIT4


[HKEY_LOCAL_MACHINE\Software\DDSwitcher]
"global"="system"
"rayman2.exe"="wine"

```

For DX8 and DX9 you can switch between HEL (there isn't native DirectX HAL) - but application needs support software/emulated rendering, Wine or Nine (Mesa DirectX 9 driver). Using Nine for DX8 (when is supported) game (Mafia):

```
REGEDIT4


[HKEY_LOCAL_MACHINE\Software\D8Switcher]
"game.exe"="ninemore"

```

Nine for DX9 game (CMR 3):

```
REGEDIT4


[HKEY_LOCAL_MACHINE\Software\D9Switcher]
"Rally_3PC.exe"="nine"

```

Supported values are: *system* (pass calls to system DLL), *wine* (using Wine9x), *nine* (use nine for DX9 when is supported), *ninemore* (Nine for DX 8 and DX 9), *nineforce* (try use Nine even when looks like unsupported) or value can be name of DLL (with path or only file name) to wrapper DLL (can be used for example with SwiftShader).

### Mesa3D

Old OpenGL games have problem with too large extension string Quake II is on of them:

```
REGEDIT4

[HKEY_LOCAL_MACHINE\Software\Mesa3D\quake2.exe]
"MESA_EXTENSION_MAX_YEAR"="2000"
```

When you using software rendering and [SIMD95](https://github.com/JHRobotics/simd95) for enable AXV instruction set, it is recommended to use them only with one application Q3 for example:

```
REGEDIT4

[HKEY_LOCAL_MACHINE\Software\Mesa3D\quake2.exe]
"LP_NATIVE_VECTOR_WIDTH"="256"
```

Possible configuration values here are: https://docs.mesa3d.org/envvars.html

### Wine

Default Wine registry path also working: https://wiki.winehq.org/Useful_Registry_Keys

But also working same registry path scheme as for Mesa, for example increase VRAM memory to 256 MB (Video RAM is emulated anyway, you can use any value when you have enough system RAM):


```
REGEDIT4

[HKEY_LOCAL_MACHINE\Software\Wine\global]
"VideoMemorySize"="256"
```


### QEMU 3DFX

```
REGEDIT4


[HKEY_LOCAL_MACHINE\Software\vmdisp9x]
; override signature in QEMU-3dfx driver
; you can obtain current signature by command:
; git rev-parse HEAD
; (only first 7 characters needed)
; you need same signature as QEMU-3dfx which you used to 
; build QEMU
;
"REV_QEMU3DFX"="3258e4a30"
;
; When you have working QEMU-3dfx this key enable
; ICD OpenGL driver
"FORCE_QEMU3DFX"=dword:00000001

```

### OpenGlide

```
REGEDIT4


[HKEY_LOCAL_MACHINE\Software\OpenGlide\Rayman2.exe]
;
; disable OpenGlide for some application
; very useful if you wish ship glide usage
; and continue for normal OpenGL
"Disabled"="1"
;
; Enable the fog, this is broken now
; but you can try it...
"FogEnable"="1"
;
; Run game in window
"InitFullScreen"="0"
;
; Convert all textures to ARGB8888 instead of
; 16-bit types. Speeds up a bit sometimes.
"Textures32bit"="1"
;
; Disable system cursor:
; 0 - cursor present
; 1 - cursor is disabled in glide window (default)
; 2 - cursor is disabled in whole system when app running
"HideCursor"="2"
;
; Disable 3DFX splash screen:
; 0 - splash is disabled for good
; 1 - always show splash screen, original 3DFX if
;     there are presents `3dfxSpl2.dll` and `3dfxSpl3.dll`
;     or OpenGlide build-in splash screen
; 2 - show only original splash screen or nothing (default)
"NoSplash"="0"
;
; Disable OpenGL vertical sync for OpenGL
; some games cannot handle it and they're too fast
;  0 - v-sync disabled
;  1 - v-sync enabled (60 FPS, default)
; -1 - v-sync adaptive (may not be supported)
"SwapInterval"="0"
;
; Scale resolution
; for example:
; 2   - 800x600 => 1600x1200
; 2.5 - 640x480 => 1600x1200
; 
"Resolution"="2"

```

You can also tune the configuration of Voodoo board

```
REGEDIT4


[HKEY_LOCAL_MACHINE\Software\OpenGlide\global]
;
; Number of texture mapping units, default is 1
; because on original HW both TMUs can be configure
; differently, but OpenGlide not support that and 
; only TMU#1 configuration is taken into account
"NumTMU"="2"
;
; Type of Voodoo board:
; -1 - auto selection (default)
;  0 - Voodoo
;  1 - Rush
;  3 - Voodoo2
;  4 - Banshee
; Default is Voodoo 2 for Glide2x and Banshee for Glide3x API
SSTType="3"
;
; Memory for one TMU in MB
; default: 8
"TextureMemorySize"="4"
;
; Memory for frame buffer in MB
; 4  - max. resolution 800x600
; 8  - max. resolution 1200x1024
; 16 - max. resolution 1600x1200
; default: 8
; original cards had 4 or 8 MB of FB memory. If you need
; large screen is better to scale resolution instead of
; use more memory
"FrameBufferMemorySize"="4"
;
; Note: OpenGlide architecture is most analogous to
;       Voodoo Banshee - FB and TMUs are in same linear       
;       memory space
;

```
