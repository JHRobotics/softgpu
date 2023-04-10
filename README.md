# SoftGPU: SW and HW accelerated driver for Windows 9x Virtual Machines

![SoftGPU animated logo](/resource/softgpu.webp)

This is ready-to-use compilation of my 4 projects:
VMDisp9x: https://github.com/JHRobotics/vmdisp9x
Mesa3D for 9x: https://github.com/JHRobotics/mesa9x
WineD3D for 9x: https://github.com/JHRobotics/wine9x
OpenGlide for 9x: https://github.com/JHRobotics/openglide9x

## Requirements
1) Virtual machine with one of these VGA adapter support:
   a) Bochs VBE (Bochs, VirtualBox, Qemu)
   b) VMware SVGA-II (VMware, VirtualBox, Qemu)
2) Windows 95/98/Me as VM guest system
   a) Windows 98/Me - required is last version of DirectX 9 (included in package)
   b) Windows 95
      - Last version of DirectX 8 (included in package)
      - Visual C runtime (version 6 included in package)
      - OpenGL 95 for versions without `opengl32.dll` (included in package)

## Installation:
0) Setup the Virtual Machine
1) Copy installation files on formatted HDD and apply **patcher9x** [Optional but recommended]
2) Install the Windows 95/98/Me [Windows 98 SE is recommended]
3) Run setup with `softgpu.exe`
4) [optional] Install audio drivers (if using AC97 soundcard) and USB (if you added USB controller)
5) [only if you have AC97 soundcard] Reinstall DirectX again = AC97 replacing some DX files, but they are not working with newer DX versions
6) Have fun!

## VirtualBox VM setup with HW acceleration
1) Create new VM selecting *Machine -> New* in menu
2) Type: Microsoft Windows, Version: Windows 98
3) Base memory: 512 MB (256 MB is minimum, but more 512 MB isn't recommended without additional patches!), CPU: 1
4) Disk size: recommended is at least 20 GB for 98/Me (you can select less, but HDD becomes full faster). Select 2 GB if you plan install Windows 95. Tip: If you storing virtual machine on classic HDD, check *Pre-allocate Full Size*, because it leads to lower disk image fragmentation.
5) Finish wizard
6) Open VM setting
- In **General** change *type* to **Linux** and *version* to **Other Linux (32-bit)** => This setting haven't any effect to hardware configuration but allow you to set GPU type through GUI.
- Now in *Display*
  - Set *Graphic Controller* to **VMSVGA**
  - set video memory to **128 MB** (VBox sometimes turn off GPU HW acceleration if this value is lower)
  - Check **enable 3D Acceleration**
7) Optional adjustment
- set USB controller to USB 1.1 (OHCI) for 98/Me, or turn USB off for 95
- Audio controller set to **SoundBlaster 16** for 95/98 or **AC 97** for Me
8) Disable VMSVGA10
- Open command line
- (on Windows) navigate to VirtualBox installation directory (default: *C:\Program Files\Oracle\VirtualBox*)
- Enter this command where *My Windows 98* is your Virtual Machine name
```
VBoxManage setextradata "My Windows 98" "VBoxInternal/Devices/vga/0/Config/VMSVGA10" "0"
```
9) Install system - Windows 98 SE is highly recommended (for newer CPU, you need my patch: https://github.com/JHRobotics/patcher9x)
10) Insert SoftGPU iso (can be downloaded in Releases) and run `softgpu.exe`
11) Click on *Install!*
12) You maybe need some reboots (after MSVCRT and DX installation) and run `softgpu.exe` again.
13) After complete and final reboot system should start in 640x480 in 256 colours
14) Right click on desktop, Properties -> Settings and set the resolution (which you wish for) and colours:
- to 32 bits for 98/Me, because only in 32 bit real HW screen acceleration works and applications are much faster
- to 16 bits for 95, because 95 can't set colour depth on runtime (reboot is required) and lots of old applications can't start in 32 bits (all Glide for example)
15) Verify settings:
- OpenGL: run `glchecker.exe` in `tools` on SoftGPU CD
  - If renderer is **SVGA3D**, you have HW acceleration, congratulation!
  - If renderer is **llvmpipe**, you have still SW acceleration, but at least accelerated by SSE (128 bits) or AVX (256 bit)
  - If renderer is **softpipe**, you have SW acceleration and running on reference (but slow) renderer, SIMD ins't accesable somehow, or you on 95, where is softpipe renderer by default, even if SIMD hack is installed (more in Mesa9x documentation: https://github.com/JHRobotics/mesa9x).
  - If program crash on loading screen, you are on MS software OpenGL renderer (run **wgltest.exe** and check value `GL_RENDERER`). If driver is correctly installed this indicates problem with loading `mesa3d.dll`, usually MSVCR missing, or you install SSE instrumented driver or Windows 95 without SIMD95.
  - If program can't start by missing `MSVCRT.DLL` install MSVCRT (part of Internet Explorer >= 4 too)
- DirectX:
  - On 98 you can run **dxdiag** (Start -> Run -> type `dxdiag`) and check all tests
  - On Me you can still run **dxdiag**, but you can only check DX8 and DX9, because we cannot easily replace system `DDRAW.DLL`. But DX6 and DX7 games should usually run without problems
  - On 95 you can still run **dxdiag**, but if you run test, you only see black screens, but again, games (if supporting 95) games should usually run

## Bugs
There are many bugs in individual components, please post them to individual repositories based on bugged application (DirectX, Glide, OpenGL).

But still, please be patient. SoftGPU compatibility target is about a decade of intensive HW and SW development (from DOS direct VGA/VESA access, SW rendering through GDI, DirectDraw, OpenGL, Glide, DirectX, OpenGL again). After all, there will still applications that cannot be run anyway because there are written for very individual SW/HW combinations.

## Compilation from source
1) You need MINGW and MAKE to create GUI setup
2) You need all development tool to compile all other component (see README.md in individual repositories)
3) Compile softgpu.exe by type make (GNU make required)
4) Compile VMDisp9x and copy files boxvmini.drv, vmwsmini.drv, vmwsmini.vxd, vmdisp9x.inf and place them to driver/win95 and driver/win98me folder
5) Compile Mesa9x for Windows 95 (e.g., without SSE) and copy and rename files to following schema
   - vmwsgl32.dll       => driver/win95/vmwsgl32.dll
   - opengl32.w98me.dll => driver/win95/extra/opengl32.dll
   - mesa3d.w98me.dll   => driver/win95/mesa3d.dll
   - glchecker.exe      => tools/glchecker.exe
   - icdtest.exe        => tools/icdtest.exe
   - wgltest.exe        => tools/wgltest.exe
   - [folder] glchecker => tools/glchecker
6) Compile Mesa9x for Windows 98 and Me (eq. with SSE, optimized for Core2 or Westmere) and copy these files
   - vmwsgl32.dll       => driver/win98me/vmwsgl32.dll
   - opengl32.w98me.dll => driver/win98me/extra/opengl32.dll
   - mesa3d.w98me.dll   => driver/win98me/mesa3d.dll
7) Compile Wine9x for Windows 95 and copy
   - ddraw.dll          => driver/win95/ddraw.dll
   - d3d8.dll           => driver/win95/d3d8.dll
   - d3d9.dll           => driver/win95/d3d9.dll
   - dwine.dll          => driver/win95/dwine.dll
   - wined3d.dll        => driver/win95/wined3d.dll
8) Compile Wine9x for Windows 98+Me and copy
   - ddraw.dll          => driver/win98me/ddraw.dll
   - d3d8.dll           => driver/win98me/d3d8.dll
   - d3d9.dll           => driver/win98me/d3d9.dll
   - dwine.dll          => driver/win98me/dwine.dll
   - wined3d.dll        => driver/win98me/wined3d.dll
9) make ddreplacer.exe (by typing make ddreplacer.exe in Wine9x)
10) Extract original ddraw.dll from DX8 redistributable for W95 and type
```
ddreplacer path/to/extracted/ddraw.dll ddr95.dll
```
   - copy ddr95.dll => driver/win95/dx/ddr95.dll
   - copy ddr95.dll => driver/win98me/dx/ddr95.dll
11) Extract original ddraw.dll from newer DX9 redistributable (doesn't matter if it's final one, this file doesn't seem to change often) and type
```
ddreplacer path/to/extracted/ddraw.dll ddr95.dll
```
   - copy ddr98.dll => driver/win95/dx/ddr98.dll
   - copy ddr98.dll => driver/win98me/dx/ddr98.dll
12) Compile OpenGlide9x for Windows 95 and copy
   - glide2x.dll        => driver/win95/glide2x.dll
   - glide3x.dll        => driver/win95/glide3x.dll
13) Compile OpenGlide9x for Windows 98 and copy
   - glide2x.dll        => driver/win98me/glide2x.dll
   - glide3x.dll        => driver/win98me/glide3x.dll
14) Edit both driver/win95/vmdisp9x.inf and driver/win98me/vmdisp9x.inf and uncomment files and that you added. CopyFiles options have to look like:
```
CopyFiles=VBox.Copy,Dx.Copy,DX.CopyBackup,Voodoo.Copy
```
and
```
CopyFiles=VBox.Copy,Dx.Copy,DX.CopyBackup,Voodoo.Copy
```
15) place redistributable to redist folder
16) Edit `softgpu.ini` for final paths review
17) Create ISO file place to it:
   - file `softgpu.exe`
   - file `softgpu.ini`
   - folder `driver`
   - folder `redist`
   - folder `tools`
   - *readme* and *licence* file
18) Mount ISO to virtual machine and enjoy it!
