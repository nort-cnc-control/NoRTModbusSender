
/*
NoRT Modbus Sender
Copyright (C) 2019-2020  Vladislav Tsendrovskii

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 3 of the License

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/*
Program description:

This program is designed for transmitting modbus frames,

Lines are received and transmitted from/to TCP:8890

How to run:

nort_mb_proxy serial_name

where serial_name is a serial interface name. ttyS0 as default
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <modbus.h>
#include <modbus-rtu.h>

int readline(int sock, char *buf)
{
    int i = 0;
    char c;
    do
    {
        int res = recv(sock, &c, 1, 0);
        if (res <= 0)
        {
            return -1;
        }
        if (c != '\n')
            buf[i++] = c;
    } while (c != '\n');
    buf[i++] = 0;
    return i-1;
}

int sendline(int sock, const char *buf, size_t len)
{
    const char cr = '\n';
    send(sock, buf, len, 0);
    send(sock, &cr, 1, 0);
    return len + 1;
}

volatile int run;
int ctlsock;
int clientctlsock;

int create_control(unsigned short port)
{
    struct sockaddr_in ctl_sockaddr;
    int ctlsock = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(ctlsock, SOL_SOCKET, SO_REUSEADDR, NULL, 0);

    ctl_sockaddr.sin_family = AF_INET;
    ctl_sockaddr.sin_port = htons(port);
    ctl_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(ctlsock, (struct sockaddr *)&ctl_sockaddr, sizeof(ctl_sockaddr)) < 0)
    {
        printf("Can not bind tcp\n");
        return -1;
    }
    return ctlsock;
}

int hex2dig(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';

    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return -1;
}

int main(int argc, const char **argv)
{
    int port = 8890;
    int i;
    
    const char* sername = "/dev/ttyUSB0";
    int serspeed = 9600;
    
    if (argc > 1)
    {
        sername = argv[1];
    }
    modbus_t *modbus = modbus_new_rtu(sername, serspeed, 'N', 8, 1);
    if (modbus == NULL)
        return -1;

    ctlsock = create_control(port);
    if (ctlsock < 0)
        return ctlsock;
    
    listen(ctlsock, 1);
    while (1)
    {
        char buf[1500];
        clientctlsock = accept(ctlsock, NULL, NULL);
        printf("Connect from client\n");
        while (1)
        {
            int len = readline(clientctlsock, buf);
            if (len < 0)
            {
                break;
            }
            printf("RECEIVE CTL: %.*s\n", len, buf);
            if (!strncmp(buf, "EXIT:", 5))
            {
                break;
            }
            else if (!strncmp(buf, "MB:", 3))
            {
                int i;
                unsigned char addrs[4], vals[4], devids[4];
                memcpy(devids, buf+3, 4);
                memcpy(addrs, buf+3+4+1, 4);
                memcpy(vals, buf+3+4+1+4+1, 4);
                for (i = 0; i < 4; i++)
                {
		    devids[i] = hex2dig(devids[i]);
		    addrs[i] = hex2dig(addrs[i]);
		    vals[i] = hex2dig(vals[i]);
                }
                unsigned addr =  addrs[0]*0x1000 +  addrs[1]*0x100 +  addrs[2]*0x10 +  addrs[3];
                unsigned val  =  vals[0]*0x1000 +   vals[1]*0x100 +   vals[2]*0x10 +   vals[3];
                unsigned devid = devids[0]*0x1000 + devids[1]*0x100 + devids[2]*0x10 + devids[3];
                printf("Device: %04X, Reg: %04X = %04X\n", devid, addr, val);
                modbus_set_slave(modbus, devid);
                if (modbus_connect(modbus) == -1)
                {
                    fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
                    modbus_free(modbus);
                    return -1;
                }
                modbus_write_register(modbus, addr, val);
            }
        }
        printf("Client disconnected\n");
        close(clientctlsock);
        clientctlsock = -1;
    }
    run = 0;
    modbus_free(modbus);
    return 0;
}
