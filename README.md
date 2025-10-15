# walkk
_an mp3 player that is actually a granulizer — turning your MP3 files into an endless stream of consciousness._

> ImGui + PortAudio. Runs on **DX11** (Windows) and **OpenGL** (Linux).


## prebuild
### ubuntu/debian
install toolchain + PortAudio + OpenGL headers + pkg-config:
```sh
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config git \
  portaudio19-dev \
  libgl1-mesa-dev
sudo apt install -y libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
sudo apt install -y python3 python3-jinja2
```

### fedora
install toolchain + PortAudio + OpenGL headers + pkg-config:
```sh
sudo dnf install -y \
  @development-tools cmake git pkgconf-pkg-config \
  portaudio-devel \
  mesa-libGL-devel
sudo dnf install -y libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel python3-jinja2
```


## building

```bash
# from repo root
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Artifacts:
CLI: build/walkk_cli (or build/Release/walkk_cli.exe)
GUI: build/walkk_gui (or build/Release/walkk_gui.exe)

Install to your system prefix:

cmake --install build

## license
MIT