WinUsbTmc
=========

Windows USBTMC (USB Test and Measurement Class) command line tool and DLL.

Sick of downloading and installing a bloated set of tools just to talk to your measurement device under windows?

These tools are intended to be a replacement for large libraries to communicate with your measurement devices 
like Scopes, Logic analyzers, Power supplies, Multimeters, etc...
Such measurement devices usually have nowadays a USB connector instead of a GPIB connector.

usb.org defined a standard communication protocol - a usb device class for that purposes which is supported by
most devices => USBTMC (USB test and measurement class).

Under Linux very lightweight tools are available to talk to such devices, but not for windows.

WinUsbTmc currently consists of the following tools:
- A usbtmc class driver (LibUsb based). This driver can be installed for all USBTMC devices and no 
  adjustments are needed to the .inf file to install a certain device.
- A standalone command line tool to talk with your devices => WinUsbTmc.Exe
  Simple to use and not dependent of any DLLs
- A Dll which can be used if you would like to use WinUsbTmc in your own projects as dynamic library. 
  Note: The command line tool does not use this library, WinUsbTmc is staticly linked
- complete sourcecode... Contribution is highly appreciated :-)


7/5/2013 Kai Gossner
xyphro@gmail.com
