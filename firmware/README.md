# ESP32 Firmware

Sensor input, Wi-Fi connectivity, trigger reporting, and retry logic belong here.

# ESP32

The ESP32 is an WROOM32 based variant which boasts dual cores, and a pretty hearty amount of resources to back it up.

Currently since the group are all using UNIX / LINUX based devices we have decided to make use of the EXPRESSIF framework and code through freeRTOS which is a industry standard framework in which tasks can be done in almost parallel through it's scheduler being able to rapdily give tasks (blocks of code) varying levels of priority to run.

As such  the following configuration has to be done in order to get the ESP32 flashed for the project.

## Install ESP-IDF on macOS (once per user)

These steps cover Intel and Apple silicon Macs running a macOS version supported by Homebrew and ESP-IDF. The project's `netserver/sdkconfig` was generated with ESP-IDF 6.1.0, so use the `v6.1` release below.

This follows Espressif's supported [manual installation method](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/linux-macos-setup-legacy.html), which provides the `export.sh` script used by the helper below. Espressif also offers the [ESP-IDF Installation Manager](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/macos-setup.html).

1. Open Terminal and install Apple's Command Line Tools, unless already installed. Finish the installer before continuing.

   ```sh
   xcode-select --install
   ```

2. Install [Homebrew](https://brew.sh/) if `brew --version` does not work:

   ```sh
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```

   Follow the installer's printed **Next steps** to add Homebrew to your shell's `PATH`. The default location is `/opt/homebrew` on Apple silicon and `/usr/local` on Intel. Verify with `brew --version` before continuing.

3. Install the prerequisites:

   ```sh
   brew install git cmake ninja dfu-util ccache python
   python3 --version
   ```

   ESP-IDF v6.1 requires Python 3.10 or newer.

4. Download ESP-IDF and its submodules into your own home directory:

   ```sh
   mkdir -p "$HOME/esp"
   cd "$HOME/esp"
   git clone --branch v6.1 --recursive https://github.com/espressif/esp-idf.git
   ```

5. Install the ESP32 toolchain and Python environment:

   ```sh
   cd "$HOME/esp/esp-idf"
   ./install.sh esp32
   ```

6. Define the `ESP` helper below, then verify activation:

   ```sh
   ESP
   idf.py --version
   ```

## Helpful thing to invoke idf globally.
This version assumes an existing installation at `/opt/esp-idf` and registers that specific directory as trusted in the user's Git configuration. For the installation under your own home directory described above, add this adapted uppercase helper instead:

```sh
esp() {
  source "$HOME/esp/esp-idf/export.sh"
}
```

Save it in `~/.zshrc` if using zsh (the default on modern macOS), or `~/.bashrc` if using Bash. For a macOS Bash login shell, also ensure `~/.bash_profile` loads `~/.bashrc`, for example with `[ -f "$HOME/.bashrc" ] && source "$HOME/.bashrc"`.

Reload the file you edited with `source ~/.zshrc` or `source ~/.bashrc`, as appropriate. The helper activates ESP-IDF in the current terminal and can be invoked from any directory. Run `esp` again in each new terminal before using `idf.py`.

If you installed ESP-IDF elsewhere, update the helper's `source` path to that installation's `export.sh`. Also this is case sensitive so running ESP or any variant not completely lowercased will not invoke it.

## Clean, build, find the port, and flash (every user)

Run these steps from the ESP-IDF project directory, `firmware/netserver`, which contains the project's `CMakeLists.txt`. Starting in the directory containing this README:

```sh
cd netserver
ESP
idf.py fullclean
idf.py build
```

Wait for each command to succeed before proceeding. `fullclean` removes the previous build output; `build` compiles the firmware again. See Espressif's [`idf.py` command reference](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/api-guides/tools/idf-py.html).

Before flashing, identify which serial device the ESP32 has bound to on your own machine. Unplug the board, run the command for your OS, reconnect it with a USB data cable, and run the same command again:

| Operating system | List serial devices | Example ESP32 port |
| --- | --- | --- |
| macOS (Intel or Apple silicon) | `ls /dev/cu.*` | `/dev/cu.usbserial-0001` or `/dev/cu.SLAB_USBtoUART` |
| Linux | `ls /dev/tty*` | `/dev/ttyUSB0` or `/dev/ttyACM0` |

Use the device that appears when the board is connected. The listings may be empty while it is unplugged. Check again after reconnecting the board; the name can change between users, machines, or USB connections. See Espressif's [serial connection guide](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/get-started/establish-serial-connection.html#check-port-on-linux-and-macos).

If your Linux board appeared as `/dev/ttyUSB0`, flash with:

```sh
idf.py -p /dev/ttyUSB0 flash
```

On macOS, substitute the port you actually found, for example:

```sh
idf.py -p /dev/cu.usbserial-0001 flash
```

Each user must follow the same order: `ESP`, `idf.py fullclean`, `idf.py build`, identify the board's port, then `idf.py -p YOUR_PORT flash`. Replace `YOUR_PORT` with the actual device path; `/dev/ttyUSB0` is a Linux example.

If no device appears, try another USB data cable and check whether the board's USB-to-UART chip needs a driver using the serial connection guide above. Close any serial monitor using the port before flashing. If automatic bootloader entry fails, hold `BOOT`, press and release `RESET` (sometimes labeled `EN`), release `BOOT`, and retry the flash command.
