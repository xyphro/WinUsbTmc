#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "winusbtmc.h"

/* small helper function to check if a string contains a numeric value*/
static bool isnumeric(const char *str)
{
    bool ret = true;
    while ((*str) && (ret))
    {
        ret = (*str >= '0') && (*str <= '9');
        str++;
    }
    return ret;
}

static void list_devices(void)
{
    int     i;
    int32_t devicecount = 0;
    char    str[256];

    devicecount = winusbtmc_get_device_count();
    printf("%d winusbtmc device(s) present.\n", devicecount);

    for (i = 0; i < devicecount; i++)
    {
        winusbtmc_get_device_string(i, (char *)str, sizeof(str));
        printf("Device #%d: \"%s\"\n", i, str);
    }
    printf("\n");
}

static void send_command(char *device, char *commandstr, bool getResponse)
{
    int devnum;
    char str[8192];
    int ret;
    bool eom;

    if ( isnumeric(device) )
    { /* device specified by device number */
        devnum = atoi(device);
    }
    else
    { /* device specified by string */
        devnum = winusbtmc_find_devnum_by_string(device);
    }

    ret = winusbtmc_send_string(devnum, commandstr);
    if (ret < 0)
    {
        printf("ERROR: %d\n", ret);
        return;
    }
    if (getResponse)
    {
        do
        {
            ret = winusbtmc_recv_string(devnum, str, sizeof(str)-1, &eom);
            if (ret < 0)
            {
                printf("ERROR: %d\n", ret);
                return;
            }
            fwrite(str, ret, 1, stdout);
            //printf(str);
        } while (!eom);
    }
}

static void interpret_command(char *cmd)
{
    if (strcasecmp(cmd, "/l") == 0)
    { /* show list of devices */
        list_devices();
    }
}

static void print_help(void)
{
    printf("\n");
    printf("simple WinUsbTmc command line tool\n");
    printf("\n");
    printf("available commands\n");
    printf("\n");
    printf("winusbtmc /L          returns a list of all available devices\n");
    printf("winusbtmc 0 \"*IDN?\"   send a command to device number 0\n");
    printf("winusbtmc \"Rigol\" \"*IDN?\"   send a command to device beginning with the string \"Rigol\"\n");
    printf("                         This will not read automatically the response of the device\n");
    printf("winusbtmc /R \"Rigol\" \"*IDN?\"   send a command to device beginning with the string \"Rigol\"\n");
    printf("                         The response is read and written to stdout. It can be redirected to a file\n");
}



int main(int argc,char *argv[])
{
    if (argc == 1)
    { /* no parameters given, return device list */
        print_help();
    }
    else if (argc == 2)
    { /* single parameter */
        interpret_command(argv[1]);
    }
    else if (argc == 3)
    { /* two parameters (device + command string */
        send_command(argv[1], argv[2], false);
    }
    else if (argc == 4)
    { /* two parameters (device + command string */
        if (strcasecmp(argv[1], "/R") == 0)
        {
            send_command(argv[2], argv[3], true);
        }
    }

    winusbtmc_deinit();

    return 0; /* \TODO: return a nonzero value in case an error occured */
}

