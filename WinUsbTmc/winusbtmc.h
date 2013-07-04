#ifndef WINUSBTMC_H_INCLUDED
#define WINUSBTMC_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>

/*
 * First version created 6/3/2012 by Kai Gossner (xyphro@gmail.com)
 *
 * This library is intended to be used to communicate with all kind of USB Test and measurement
 * devices, e.g. Scopes, Function generators, Spectrum analyzers, Multimeters, etc...
 *
 * It uses the windows port of LibUsb. LibUsb is linked as static library, so libusb.dll is not needed.
 * The USB VID/PID does not need to be matched to your current device, because the libusb .inf
 * file is adjusted, so that it matches to all USBTMC class devices.
 *
 * This module can be directly included to your project.
 * When defining BUILD_DLL, you can build this module as a windows Dynamic Link Library
 *
 */

#ifdef BUILD_DLL
    #define DLL_EXPORT __declspec(dllexport) __stdcall
#else
    #define DLL_EXPORT
#endif

/*
 * USBTMC Error codes
 * The functions in this module which have a signed integer return value indicate an error
 * for negative return values. These are the possible error codes.
 */

#define WINUSBTMC_ERR_NONE                0
#define WINUSBTMC_ERR_CANNOT_OPEN_DEVICE -1
#define WINUSBTMC_ERR_MALLOC_FAILED      -2
#define WINUSBTMC_ERR_DEVICE_NOT_PRESENT -3
#define WINUSBTMC_ERR_FIRST_INIT_FAILED  -4
#define WINUSBTMC_ERR_BULKOUT_FAILED     -5
#define WINUSBTMC_ERR_BULKIN_FAILED      -6
#define WINUSBTMC_ERR_INVALID_PARAMETER  -7


#ifdef __cplusplus
extern "C"
{
#endif



/**************************************************************************************************
 * functions to initialize / deinitialize this module
 **************************************************************************************************/

/* [winusbtmc_init]
 *
 * Initialize the usbtmc module.
 * This function does not need to be called. It will be internally called when needed.
 * In case you plugged in a new usbtmc after the module was used you can call this function
 * to make usbtmc detect the new device.
 */
DLL_EXPORT void          winusbtmc_init (void);


/* [winusbtmc_deinit]
 *
 * Deinitialize this module. The module allocates some dynamic memory from heap.
 * When calling this function this memory is deallocated and all opened usbtmc devices closed.
 */
DLL_EXPORT void          winusbtmc_deinit (void);



/**************************************************************************************************
 * functions to identify or find available devices
 **************************************************************************************************/

/* [winusbtmc_get_device_count]
 *
 * Get the count of available usbtmc devices in the system
 */
DLL_EXPORT int32_t       winusbtmc_get_device_count (void);

/* [winusbtmc_get_device_string]
 *
 * Get the device string of the device "devnum".
 * set strln to the max. allowed size of the string.
 */
DLL_EXPORT void          winusbtmc_get_device_string (int32_t devnum, char *str, int strln);

/* [winusbtmc_find_devnum_by_string]
 *
 * get the devicenumber of a device based on its string, this can be e.g.:
 * "Rigol Technologies:DS1000 SERIES:DS1EB1501xxxxx"
 * You can pass also a shorter version of this string, e.g.
 * "Rigol Technologies:DS1000" would also find the same device
 */
DLL_EXPORT int32_t       winusbtmc_find_devnum_by_string(const char *pstr);




/**************************************************************************************************
 * functions to exchange data with the device
 **************************************************************************************************/

/* [winusbtmc_send_string]
 *
 * Send a command to the usbtmc device, e.g. "*IDN?".
 * The command is automatically terminated with 0x0a (\n)
 */
DLL_EXPORT int32_t       winusbtmc_send_string(int32_t devnum, const char *str);

/* [winusbtmc_recv_string]
 *
 * receive a response string from the usbtmc device.
 * set maxlen to the max. allowed size of the string.
 * eom indicates if the response message was completely received, if *eom is returned as false,
 *   call this function until *eom is true.
 * This function removes the termination 0x0a character from the end of the string
 *   in case it was received (this is the only difference to the function winusbtmc_recv_data.
 */
DLL_EXPORT int32_t       winusbtmc_recv_string(int32_t devnum, char *str, uint32_t maxlen, bool *eom);

/* [winusbtmc_recv_string]
 *
 * receive a response as data from the usbtmc device, e.g. a screenshot.
 * set maxlen to the max. allowed size of the string
 * eom indicates if the response message was completely received, if *eom is returned as false,
 *   call this function until *eom is true
 */
DLL_EXPORT int32_t       winusbtmc_recv_data(int32_t devnum, char *dat, uint32_t maxlen, bool *eom);

#ifdef __cplusplus
}
#endif

#endif // WINUSBTMC_H_INCLUDED
