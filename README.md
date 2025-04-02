# GameInn Cocos POC


### Prerequisites
In Ubuntu 18.04 (LTS) following packages are required:

- cmake
- libfontconfig1-dev
- libgtk-3-dev
- libglew-dev
- libcurl4-openssl-dev
- libsqlite3-dev

```
sudo apt update && sudo apt install cmake libfontconfig1-dev libgtk-3-dev libglew-dev libcurl4-openssl-dev sqlite3 libsqlite3-dev 
```

###
Uinput device drivers have to be compiled from [uinputd](https://gitlab.omni-chip.com/gameinn/uinputd) and correctly linked:

```
ln -s <path to uinputd> libgameinn
```

### Build instructions

```
cmake .
make
```

### Building Android projects
In Ubuntu 19.04 install:
 - Android NDK
 - Android Studio (via snap package)

````
sudo apt update && sudo apt install snapd
sudo snap install android-studio --classic
sudo apt install google-android-ndk-installer
````

## License

Copyright (c) 2024 OmniChip Sp. z o.o.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.