# libble++

Implementation of Bluetooth Low Energy functions in modern C++, without
the BlueZ DBUS API.

### Includes
* Scanning for bluetooth packets
* Implementation of the GATT profile and ATT protocol
* Lots of comments, complete with references to the specific part of
  the Bluetooth 4.0 standard.
* Example programs

### Design
Clean, modern C++ with callbacks. Feed it with lambdas (or whatever you like)
to perform an event happens. Access provided to the raw socket FD, so 
you can use select(), poll() or blocking IO.

### The example programs
* lescan_simple: Simplest possible program for scanning for devices. Only 2 non boilerplate lines.
* lescan: A "proper" scanning program that cleans up properly. It's got the same 2 lines of BLE related code and a bit of pretty standard unix for dealing with non blocking I/O and signals.
* temperature: A program for logging temperature values from a device providing a standard temperature characteristic. Very short to indicate the usave, but not much error checking.


### Building the library
There are currently autoconf (./configure) and CMake options. It's not
a complex library to build, so either option should work fine.  
Autoconf:
```
./configure  
make
```  
CMake:
```
mkdir build && cd build
cmake ..
make install
```
CMake with examples:  
Examples will be in ```build/examples```
```
mkdir build && cd build
cmake -DWITH_EXAMPLES=ON ..
make install
```