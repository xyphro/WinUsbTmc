#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lusb0_usb.h"
#include "winusbtmc.h"

#define WINUSBTMC_USTR_MAX (256) /* maximum length of the unique identifier string */
#define WINUSBTMC_MAX_DEVNUM (128) /* max. count of supported devices */

#define WINUSBTMC_CLASS    (0xfe)
#define WINUSBTMC_SUBCLASS (0x03)

#define WINUSBTMC_DEV_DEP_MSG_OUT (0x01)
#define WINUSBTMC_DEV_DEP_MSG_IN  (0x02)

#define WINUSBTMC_TIMEOUT (1000)              /* timeout, unit milliseconds */


typedef struct
{
    struct usb_device *dev;                /* usb device structure */
    usb_dev_handle    *usb_handle;         /* handle to usb device (0 if not opened) */
    int8_t             usb_config;         /* configuration number */
    int8_t             usb_interface;      /* interface number */
    int8_t             usb_alt_setting;    /* alternative setting (-1 if none) */
    char               usb_uniquestring[WINUSBTMC_USTR_MAX];

    uint8_t            usb_ep_bulkin;
    uint8_t            usb_ep_bulkout;
    uint8_t            usb_ep_interrupt;

    /* status information for usbtmc protocol handling */
    uint8_t            winusbtmc_bTag;        /* current bTag number (incremented each transfer) */
    uint8_t            winusbtmc_status;      /* latest status code from usbtmc device */
} winusbtmc_device_t;

typedef winusbtmc_device_t *winusbtmc_device_ptr_t;

typedef struct
{
    uint8_t MsgID;
    uint8_t bTag;
    uint8_t bTagInverse;
    uint8_t Rsvd1;
    uint32_t TransferSize;
    uint8_t bmTransferAttributes;
    uint8_t Rsvd2;
    uint8_t Rsvd3;
    uint8_t Rsvd4;
} winusbtmc_bulkout_header_t;

/**************************************************************************************************
 * Global variables
 **************************************************************************************************/



bool s_winusbtmc_initialized = false;                              /* used to detect if this module was initialized */
winusbtmc_device_ptr_t g_winusbtmc_deviceinfo_ptr[WINUSBTMC_MAX_DEVNUM]; /* this stores information about opened devices*/





/**************************************************************************************************
 * Private functions
 **************************************************************************************************/




/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
static size_t strlcat(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (n-- != 0 && *d != '\0')
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return(dlen + strlen(s));

	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return(dlen + (s - src));	/* count does not include NUL */
}

size_t strlcpy(char * dst, const char * src, size_t siz)
{
    char *d = dst;
    const char *s = src;
    size_t n = siz;

    /* Copy as many bytes as will fit */
    if (n != 0)
    {
        while (--n != 0)
        {
            if ((*d++ = *s++) == '\0')
            break;
        }
    }
    /* Not enough room in dst, add NUL and traverse rest of src */
    if (n == 0)
    {
        if (siz != 0)
            *d = '\0'; /* NUL-terminate dst */
        while (*s++)
        ;
    }
    return(s - src - 1); /* count does not include NUL */
}

static void strtrim(char * s) {
    char * p = s;
    int l = strlen(p);

    while(p[l - 1] == ' ')
        p[--l] = 0;
    while(* p && (*p == ' '))
        ++p, --l;

    memmove(s, p, l + 1);
}



/*
 * walk through all present usbtmc devices.
 * returns count of present usbtmc devices and returns some device information if requested
 * returns -1 in case an error occured
 */
static int32_t s_winusbtmc_deviceindex(int32_t devicenum, winusbtmc_device_t *pdeviceinfo)
{
    struct usb_bus *bus;
    int c, i, a, e;
    uint32_t devicecount;
    bool     error = false;
    struct usb_endpoint_descriptor *endpoint;
    usb_dev_handle *udev;
    char     str[WINUSBTMC_USTR_MAX];


    devicecount = 0;

    /* Walk through all devices associated with libusb driver
     * YES, this is real spaghetti code :-) */
    for (bus = usb_get_busses(); bus; bus = bus->next)
    {
        struct usb_device *dev;

        /* Loop through all devices */
        for (dev = bus->devices; dev; dev = dev->next)
        {
            /* Loop through all of the configurations */
            for (c = 0; c < dev->descriptor.bNumConfigurations; c++)
            {
                /* Loop through all of the interfaces */
                for (i = 0; i < dev->config[c].bNumInterfaces; i++)
                {
                    /* Loop through all of the alternate settings */
                    for (a = 0; a < dev->config[c].interface[i].num_altsetting; a++)
                    {
                        /* Check if this interface is a usbtmc if */
                        if ( (dev->config[c].interface[i].altsetting[a].bInterfaceClass == 0xfe) &&
                             (dev->config[c].interface[i].altsetting[a].bInterfaceSubClass == 0x03) &&
                             ( (dev->config[c].interface[i].altsetting[a].bInterfaceProtocol == 0x00) ||
                               (dev->config[c].interface[i].altsetting[a].bInterfaceProtocol == 0x01))   )
                        {
                            if ((devicenum == devicecount) && (pdeviceinfo) )
                            {
                                memset(pdeviceinfo, 0, sizeof(winusbtmc_device_t));
                                pdeviceinfo->dev              = dev;
                                pdeviceinfo->usb_handle       = (void *)0;
                                pdeviceinfo->usb_config       = dev->config[c].bConfigurationValue;
                                pdeviceinfo->usb_interface    = i;
                                pdeviceinfo->usb_alt_setting  = a;
                                pdeviceinfo->usb_ep_bulkin    = -1;
                                pdeviceinfo->usb_ep_bulkout   = -1;
                                pdeviceinfo->usb_ep_interrupt = -1;

                                /* identify endpoints (bulk in, bulk out, interrupt in) */
                                for (e = 0; e < dev->config[c].interface[i].altsetting[a].bNumEndpoints; e++)
                                {
                                    endpoint = &dev->config[c].interface[i].altsetting[a].endpoint[e];
                                    switch (endpoint->bmAttributes)
                                    {
                                        case USB_ENDPOINT_TYPE_BULK: /* bulk in or out => identify direction */
                                            if (endpoint->bEndpointAddress & (USB_ENDPOINT_DIR_MASK))
                                            { /* bit 7 is set => IN endpoint*/
                                                pdeviceinfo->usb_ep_bulkin = endpoint->bEndpointAddress;
                                            }
                                            else
                                            {
                                                pdeviceinfo->usb_ep_bulkout = endpoint->bEndpointAddress;
                                            }
                                            break;
                                        case USB_ENDPOINT_TYPE_INTERRUPT:
                                            if (endpoint->bEndpointAddress & (USB_ENDPOINT_DIR_MASK))
                                            { /* bit 7 is set => IN endpoint*/
                                                pdeviceinfo->usb_ep_interrupt = endpoint->bEndpointAddress;
                                            }
                                            break;
                                        default:
                                            break;
                                    }
                                }

                                /* retrieve USB string descriptors */
                                udev = usb_open(dev);
                                if (udev)
                                {
                                    if (dev->descriptor.iManufacturer)
                                    {
                                        if (usb_get_string_simple(udev, dev->descriptor.iManufacturer, str, WINUSBTMC_USTR_MAX - 1) > 0)
                                        {
                                            strtrim(str);
                                            strlcat(pdeviceinfo->usb_uniquestring, str, WINUSBTMC_USTR_MAX);
                                        }
                                    }
                                    strlcat(pdeviceinfo->usb_uniquestring, ":", WINUSBTMC_USTR_MAX);
                                    if (dev->descriptor.iProduct)
                                    {
                                        if (usb_get_string_simple(udev, dev->descriptor.iProduct, str, WINUSBTMC_USTR_MAX - 1) > 0)
                                        {
                                            strtrim(str);
                                            strlcat(pdeviceinfo->usb_uniquestring, str, WINUSBTMC_USTR_MAX);
                                        }

                                    }
                                    strlcat(pdeviceinfo->usb_uniquestring, ":", WINUSBTMC_USTR_MAX);
                                    if (dev->descriptor.iSerialNumber)
                                    {
                                        if (usb_get_string_simple(udev, dev->descriptor.iSerialNumber, str, WINUSBTMC_USTR_MAX - 1) > 0)
                                        {
                                            strtrim(str);
                                            strlcat(pdeviceinfo->usb_uniquestring, str, WINUSBTMC_USTR_MAX);
                                        }
                                    }
                                    usb_close(udev);
                                }
                                else
                                {
                                    error = true; /* device can not be opened */
                                }

                            }
                            devicecount++;
                        }
                    }
                }
            }
        }
    }
    if ( (devicenum >= 0) && (pdeviceinfo) && (devicenum >= devicecount) )
        error = true;

    if (error)
        return -1;
    else
        return devicecount;
}


/*
 * Do initialization when device is opened the first time within this module
 */
static int32_t s_winusbtmc_first_open_init(int32_t devnum)
{
    char  dat[200];
    int     ret;


    if (usb_claim_interface(g_winusbtmc_deviceinfo_ptr[devnum]->usb_handle, g_winusbtmc_deviceinfo_ptr[devnum]->usb_interface) < 0)
    {
        return WINUSBTMC_ERR_FIRST_INIT_FAILED;
    }

    if (usb_set_configuration(g_winusbtmc_deviceinfo_ptr[devnum]->usb_handle, g_winusbtmc_deviceinfo_ptr[devnum]->usb_config) < 0)
    {
        return WINUSBTMC_ERR_FIRST_INIT_FAILED;
    }

 /*   if (usb_set_altinterface(g_winusbtmc_deviceinfo_ptr[devnum]->usb_handle, g_winusbtmc_deviceinfo_ptr[devnum]->usb_alt_setting) < 0)
    {
        return WINUSBTMC_ERR_FIRST_INIT_FAILED;
    }*/



    /* get capabilities */
    ret = usb_control_msg(g_winusbtmc_deviceinfo_ptr[devnum]->usb_handle, USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_ENDPOINT_IN,
                      7, /*  */
                      0,  /* value */
                      g_winusbtmc_deviceinfo_ptr[devnum]->usb_interface,  /* interface id */
                      dat, 0x18, WINUSBTMC_TIMEOUT);



    /* unkown request */
    ret = usb_control_msg(g_winusbtmc_deviceinfo_ptr[devnum]->usb_handle, USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_ENDPOINT_IN,
                      0xa0, /*  */
                      1,  /* value */
                      g_winusbtmc_deviceinfo_ptr[devnum]->usb_interface,  /* interface id */
                      dat, 0x1, WINUSBTMC_TIMEOUT);




    #if 0
    /* commented out, because these class commands are not implemented in some usbtmc class devices,
       allthough they are mandatory according to the USBTMC standard.
       One example for such devices is the Rigol DS2000 series (DS1000 supports them well!) */
    /* send INITIATE_CLEAR class command */
    ret = usb_control_msg(g_winusbtmc_deviceinfo_ptr[devnum]->usb_handle,
                          USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_ENDPOINT_IN,
                          6, /* check_clear_status */
                          0,  /* value */
                          g_winusbtmc_deviceinfo_ptr[devnum]->usb_interface,  /* interface id */
                          dat,
                          2,
                          WINUSBTMC_TIMEOUT);

    if (ret < 0)
    {
        return WINUSBTMC_ERR_FIRST_INIT_FAILED;
    }

    /* send CHECK_CLEAR_STATUS class command*/
    ret = usb_control_msg(g_winusbtmc_deviceinfo_ptr[devnum]->usb_handle,
                          USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_ENDPOINT_IN,
                          6, /* check_clear_status */
                          0,  /* value */
                          g_winusbtmc_deviceinfo_ptr[devnum]->usb_interface,  /* interface id */
                          dat,
                          2,
                          WINUSBTMC_TIMEOUT);
    if (ret < 0)
    {
        return WINUSBTMC_ERR_FIRST_INIT_FAILED;
    }
    usb_resetep(g_winusbtmc_deviceinfo_ptr[devnum]->usb_handle, 0x01);
    #endif

    return WINUSBTMC_ERR_NONE;
}

static int32_t s_winusbtmc_device_open(int32_t devnum)
{
    int32_t ret;
    bool    firstopen;
    winusbtmc_device_ptr_t pdevinfo;

    firstopen = false;

    /* allocate memory for deviceinfo structure and do initial prefill */
    if (!g_winusbtmc_deviceinfo_ptr[devnum])
    { /* we come here if the devices was opened the very first time */
        firstopen = true;
        pdevinfo = malloc(sizeof(winusbtmc_device_t));
        if (!pdevinfo)
        {
            return WINUSBTMC_ERR_MALLOC_FAILED;
        }

        g_winusbtmc_deviceinfo_ptr[devnum] = pdevinfo;
        ret = s_winusbtmc_deviceindex(devnum, pdevinfo);
        if (ret < 0 )
        {
            free(g_winusbtmc_deviceinfo_ptr[devnum]);
            g_winusbtmc_deviceinfo_ptr[devnum] = (void *)0;
            return WINUSBTMC_ERR_CANNOT_OPEN_DEVICE;
        }
    }

    /* check if device is already open */
    if (!g_winusbtmc_deviceinfo_ptr[devnum]->usb_handle)
    {
        g_winusbtmc_deviceinfo_ptr[devnum]->usb_handle = usb_open(g_winusbtmc_deviceinfo_ptr[devnum]->dev);

        if (!g_winusbtmc_deviceinfo_ptr[devnum]->usb_handle)
        {
            return WINUSBTMC_ERR_CANNOT_OPEN_DEVICE;
        }
    }

    /* device is opened */
    if (firstopen)
    { /* device is opened the first time... do some initialization */
        ret = s_winusbtmc_first_open_init(devnum);
        if (ret < 0)
            return ret;
    }


    return WINUSBTMC_ERR_NONE;
}

/*
 * eom gets true, if the complete message was received
 * maxlen = max. amount of data to receive
 * returns the count of received bytes
 */
static int32_t s_winusbtmc_receive_data(int32_t devnum, char *str, uint32_t maxlen, bool *eom)
{
    char *dat;
    winusbtmc_bulkout_header_t hdr;
    int ret;
    uint32_t reclen;

    dat = malloc(maxlen + sizeof(winusbtmc_bulkout_header_t) + 4);
    if (!dat)
    {
        return WINUSBTMC_ERR_MALLOC_FAILED;
    }

    hdr.MsgID        = WINUSBTMC_DEV_DEP_MSG_IN;
    hdr.bTag         = g_winusbtmc_deviceinfo_ptr[devnum]->winusbtmc_bTag++;
    hdr.bTagInverse  = hdr.bTag ^ 0xff;
    hdr.Rsvd1        = 0;
    hdr.TransferSize = maxlen;
    hdr.bmTransferAttributes = 0x00;
    hdr.Rsvd2        = 0;
    hdr.Rsvd3        = 0;
    hdr.Rsvd4        = 0;
    ret = usb_bulk_write( g_winusbtmc_deviceinfo_ptr[devnum]->usb_handle,
                          g_winusbtmc_deviceinfo_ptr[devnum]->usb_ep_bulkout,
                          (char *)&hdr, sizeof(hdr), WINUSBTMC_TIMEOUT);

    if (ret < 0)
    {
        free(dat);
        return WINUSBTMC_ERR_BULKOUT_FAILED;
    }
    ret = usb_bulk_read( g_winusbtmc_deviceinfo_ptr[devnum]->usb_handle,
                         g_winusbtmc_deviceinfo_ptr[devnum]->usb_ep_bulkin,
                         dat, maxlen + sizeof(winusbtmc_bulkout_header_t) + 4, WINUSBTMC_TIMEOUT);

    if (ret < 0)
    {
        free(dat);
        return WINUSBTMC_ERR_BULKIN_FAILED;
    }

    reclen = ((winusbtmc_bulkout_header_t*)dat)->TransferSize;
    memcpy(str, &dat[sizeof(winusbtmc_bulkout_header_t)], reclen);

    if (eom)
    {
        *eom = ( ((winusbtmc_bulkout_header_t*)dat)->bmTransferAttributes == 0x01 );
    }

    free(dat);
    return reclen;
}

/* Initializes the module automatically if needed and opens device if devnum >= 0*/
int32_t s_winusbtmc_preinitcheck(int32_t devnum)
{
    if ((devnum < -1) | (devnum >= WINUSBTMC_MAX_DEVNUM))
    {
        return WINUSBTMC_ERR_INVALID_PARAMETER;
    }

    if (!s_winusbtmc_initialized)
    {
        winusbtmc_init();
    }

    if (devnum >= 0)
        return s_winusbtmc_device_open(devnum);

    return WINUSBTMC_ERR_NONE;
}


/**************************************************************************************************
 * Public functions
 **************************************************************************************************/



DLL_EXPORT void winusbtmc_init(void)
{
    s_winusbtmc_initialized = true;
    memset(g_winusbtmc_deviceinfo_ptr, 0, sizeof(g_winusbtmc_deviceinfo_ptr));

    usb_init();                            /* initialize the library */
    usb_find_busses();                     /* find all busses */
    usb_find_devices();                    /* find all connected devices */
}

DLL_EXPORT void winusbtmc_deinit(void)
{
    int i;

    for (i=0; i < WINUSBTMC_MAX_DEVNUM; i++)
    {
        if (g_winusbtmc_deviceinfo_ptr[i])
        {
            if (g_winusbtmc_deviceinfo_ptr[i]->usb_handle)
            {
                usb_release_interface(g_winusbtmc_deviceinfo_ptr[i]->usb_handle, g_winusbtmc_deviceinfo_ptr[i]->usb_interface);
                usb_close(g_winusbtmc_deviceinfo_ptr[i]->usb_handle);
            }
            free(g_winusbtmc_deviceinfo_ptr[i]);
            g_winusbtmc_deviceinfo_ptr[i] = (void *)0;
        }
    }
}


DLL_EXPORT int32_t winusbtmc_get_device_count(void)
{
    int32_t ret;

    ret = s_winusbtmc_preinitcheck(-1);
    if (ret < 0)
    {
        return ret;
    }
    return s_winusbtmc_deviceindex(-1, 0);
}


DLL_EXPORT void winusbtmc_get_device_string(int32_t devnum, char *str, int strln)
{
    winusbtmc_device_t devinfo;
    int32_t ret;

    ret = s_winusbtmc_preinitcheck(-1);
    if (ret < 0)
    {
        return;
    }

    if (s_winusbtmc_deviceindex(devnum, &devinfo))
    {
        strlcpy(str, devinfo.usb_uniquestring, strln);
    }
    else if (strln > 0)
    {
        *str = '\0';
    }
}


DLL_EXPORT int32_t winusbtmc_find_devnum_by_string(const char *pstr)
{
    winusbtmc_device_t devinfo;
    int32_t         devcnt, i, err;
    bool            found;
    int32_t         ret;

    ret = s_winusbtmc_preinitcheck(-1);
    if (ret < 0)
    {
        return ret;
    }

    devcnt = winusbtmc_get_device_count();
    if (devcnt < 0)
    {
        return WINUSBTMC_ERR_DEVICE_NOT_PRESENT;
    }

    i = 0;
    do
    {
        err = s_winusbtmc_deviceindex(i, &devinfo);
        if ( strlen(devinfo.usb_uniquestring) > strlen(pstr) )
        {
            devinfo.usb_uniquestring[strlen(pstr)] = '\0';
        }
        found  = ( 0 == strcasecmp(devinfo.usb_uniquestring, pstr) );
        i++;
    }
    while ( (i < devcnt) && (err >= 0) && (!found) );

    if (found)
    {
        return --i;
    }
    else
    {
        return WINUSBTMC_ERR_DEVICE_NOT_PRESENT;
    }
}


DLL_EXPORT int32_t winusbtmc_send_string(int32_t devnum, const char *str)
{
    char     *dat;
    uint32_t  len;
    int       ret;

    ret = s_winusbtmc_preinitcheck(devnum);
    if (ret < 0)
    {
        return ret;
    }

    len = strlen(str);
    dat = malloc(len + sizeof(winusbtmc_bulkout_header_t) + 5);

    if (!dat)
    {
        return WINUSBTMC_ERR_MALLOC_FAILED;
    }

    ((winusbtmc_bulkout_header_t*)dat)->MsgID = WINUSBTMC_DEV_DEP_MSG_OUT;
    ((winusbtmc_bulkout_header_t*)dat)->bTag  = g_winusbtmc_deviceinfo_ptr[devnum]->winusbtmc_bTag++;
    ((winusbtmc_bulkout_header_t*)dat)->bTagInverse = ((winusbtmc_bulkout_header_t*)dat)->bTag ^ 0xff;
    ((winusbtmc_bulkout_header_t*)dat)->Rsvd1 = 0;
    ((winusbtmc_bulkout_header_t*)dat)->TransferSize = len;
    ((winusbtmc_bulkout_header_t*)dat)->bmTransferAttributes = 0x01; // EOM set
    ((winusbtmc_bulkout_header_t*)dat)->Rsvd2 = 0;
    ((winusbtmc_bulkout_header_t*)dat)->Rsvd3 = 0;
    ((winusbtmc_bulkout_header_t*)dat)->Rsvd4 = 0;

    memcpy(&dat[sizeof(winusbtmc_bulkout_header_t)], str, len);

    /* add terminator 0x0a if not present */
    if (dat[sizeof(winusbtmc_bulkout_header_t)+len-1] != 0x0a)
    {
        dat[sizeof(winusbtmc_bulkout_header_t)+len] = 0x0a;
        len++;
        ((winusbtmc_bulkout_header_t*)dat)->TransferSize++;
    }

    len += sizeof(winusbtmc_bulkout_header_t);

    /* 32 Bit zero padding */
    while ((len & 0x03) != 0)
    {
        dat[len++] = 0x00;
    }

    ret = usb_bulk_write( g_winusbtmc_deviceinfo_ptr[devnum]->usb_handle,
                          g_winusbtmc_deviceinfo_ptr[devnum]->usb_ep_bulkout, dat, len, WINUSBTMC_TIMEOUT);

    free(dat);

    if (ret < 0)
    {
        return WINUSBTMC_ERR_BULKOUT_FAILED;
    }

    return WINUSBTMC_ERR_NONE;
}

DLL_EXPORT int32_t winusbtmc_recv_data(int32_t devnum, char *dat, uint32_t maxlen, bool *eom)
{
    int32_t ret;
    ret = s_winusbtmc_preinitcheck(devnum);
    if (ret < 0)
    {
        return ret;
    }
    return s_winusbtmc_receive_data(devnum, dat, maxlen, eom);
}

DLL_EXPORT int32_t winusbtmc_recv_string(int32_t devnum, char *str, uint32_t maxlen, bool *eom)
{
    int32_t ret;

    ret = s_winusbtmc_preinitcheck(devnum);
    if (ret < 0)
    {
        return ret;
    }

    ret = s_winusbtmc_receive_data(devnum, str, maxlen-1, eom);
    if ( (ret > 0) && (*eom) )
    {
        if (str[ret-1] = '\n')
        {
            str[ret-1] = '\0';
        }
        else
        {
            str[ret] = '\0';
        }
    }
    return ret;
}


