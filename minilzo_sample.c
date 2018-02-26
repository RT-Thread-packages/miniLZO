/*
 * File : minilzo_sample.c
 * this example is a very simple test program for the minilzo library,
 * using non-stream compress and decompress. If you want to use stream compress,
 * you need at least 100K of ROM for history buffer(not recommend), or you can custom  
 * header to storage the compress block size, and carry out stream compress by non-stream.
 *
 * COPYRIGHT (C) 2006 - 2018, RT-Thread Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Change Logs:
 * Date          Author          Notes
 * 2018-02-05    chenyong     first version
 * 2018-02-11    Murphy        Adapted minilzo
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rtthread.h>

#include <dfs_posix.h>

#include "minilzo.h"

#define malloc     rt_malloc
#define free       rt_free

#define BLOCK_HEADER_SIZE              4

/* The output buffer can not be smaller than 66 bytes */
#define COMPRESS_BUFFER_SIZE           4096
#define DCOMPRESS_BUFFER_SIZE          4096

/*  we must provide a little more output space in case that compression is not possible */
#define COMPRESS_BUFFER_PADDING        (COMPRESS_BUFFER_SIZE / 16 + 64 + 3)

static int minilzo_compress_file(int fd_in, int fd_out)
{
    /* Start to compress file  */
    rt_uint8_t *cmprs_buffer = RT_NULL, *buffer = RT_NULL;
    rt_uint8_t buffer_hdr[BLOCK_HEADER_SIZE] = { 0 };
    int cmprs_size = 0, block_size = 0, totle_cmprs_size = 0;
    size_t file_size = 0, i = 0;
    int ret = 0;

    /* Work-memory needed for compression */
    lzo_voidp wrkmem = (lzo_voidp)malloc(LZO1X_1_MEM_COMPRESS);
    memset(wrkmem, 0x00, LZO1X_1_MEM_COMPRESS);

    file_size = lseek(fd_in, 0, SEEK_END);
    lseek(fd_in, 0, SEEK_SET);

    cmprs_buffer = (rt_uint8_t *) malloc(COMPRESS_BUFFER_SIZE + COMPRESS_BUFFER_PADDING);
    buffer = (rt_uint8_t *) malloc(COMPRESS_BUFFER_SIZE);
    if (!cmprs_buffer || !buffer)
    {
        rt_kprintf("[minilzo] No memory for cmprs_buffer or buffer!\n");
        ret = -1;
        goto _exit;
    }

    rt_kprintf("[minilzo]compress start : ");
    for (i = 0; i < file_size; i += COMPRESS_BUFFER_SIZE)
    {
        if ((file_size - i) < COMPRESS_BUFFER_SIZE)
        {
            block_size = file_size - i;
        }
        else
        {
            block_size = COMPRESS_BUFFER_SIZE;
        }
        
        memset(buffer, 0x00, COMPRESS_BUFFER_SIZE);
        memset(cmprs_buffer, 0x00, COMPRESS_BUFFER_SIZE + COMPRESS_BUFFER_PADDING);

        read(fd_in, buffer, block_size);

        /* The destination buffer must be at least size + 400 bytes large because incompressible data may increase in size. */
        ret = lzo1x_1_compress(buffer, block_size, cmprs_buffer, &cmprs_size, wrkmem);
        if (ret != LZO_E_OK)
        {
            ret = -1;
            goto _exit;            
        }

        /* Store compress block size to the block header (4 byte). */
        buffer_hdr[3] = cmprs_size % (1 << 8);
        buffer_hdr[2] = (cmprs_size % (1 << 16)) / (1 << 8);
        buffer_hdr[1] = (cmprs_size % (1 << 24)) / (1 << 16);
        buffer_hdr[0] = cmprs_size / (1 << 24);

        write(fd_out, buffer_hdr, BLOCK_HEADER_SIZE);
        write(fd_out, cmprs_buffer, cmprs_size);

        totle_cmprs_size += cmprs_size + BLOCK_HEADER_SIZE;
        rt_kprintf(">");
    }

    rt_kprintf("\n");
    rt_kprintf("[minilzo]compressed %d bytes into %d bytes , compression ratio is %d%!\n", file_size, totle_cmprs_size,
            (totle_cmprs_size * 100) / file_size);
_exit:
    if (cmprs_buffer)
    {
        free(cmprs_buffer);
    }

    if (buffer)
    {
        free(buffer);
    }

    if (wrkmem)
    {
        free(wrkmem);
    }

    return ret;
}


static int minilzo_decompress_file(int fd_in, int fd_out)
{
    /* Start to decompress file  */
    rt_uint8_t *dcmprs_buffer = RT_NULL, *buffer = RT_NULL;
    rt_uint8_t buffer_hdr[BLOCK_HEADER_SIZE] = { 0 };
    size_t dcmprs_size = 0, block_size = 0, total_dcmprs_size = 0;
    size_t file_size = 0, i = 0;
    int ret = 0;

    file_size = lseek(fd_in, 0, SEEK_END);
    lseek(fd_in, 0, SEEK_SET);

    if (file_size <= BLOCK_HEADER_SIZE)
    {
        rt_kprintf("[minilzo] decomprssion file size : %d error!\n", file_size);
        ret = -1;
        goto _dcmprs_exit;
    }

    dcmprs_buffer = (rt_uint8_t *) malloc(DCOMPRESS_BUFFER_SIZE);
    buffer = (rt_uint8_t *) malloc(DCOMPRESS_BUFFER_SIZE + COMPRESS_BUFFER_PADDING);
    if (!dcmprs_buffer || !buffer)
    {
        rt_kprintf("[minilzo] No memory for dcmprs_buffer or buffer!\n");
        ret = -1;
        goto _dcmprs_exit;
    }

    rt_kprintf("[minilzo]decompress start : ");
    for (i = 0; i < file_size; i += BLOCK_HEADER_SIZE + block_size)
    {
        /* Get the decompress block size from the block header. */
        read(fd_in, buffer_hdr, BLOCK_HEADER_SIZE);
        block_size = buffer_hdr[0] * (1 << 24) + buffer_hdr[1] * (1 << 16) + buffer_hdr[2] * (1 << 8) + buffer_hdr[3];

        memset(buffer, 0x00, COMPRESS_BUFFER_SIZE + COMPRESS_BUFFER_PADDING);
        memset(dcmprs_buffer, 0x00, DCOMPRESS_BUFFER_SIZE);

        read(fd_in, buffer, block_size);

        ret = lzo1x_decompress(buffer, block_size, dcmprs_buffer, &dcmprs_size, NULL);
        if (ret != LZO_E_OK)
        {
            ret = -1;
            goto _dcmprs_exit;            
        }
        
        write(fd_out, dcmprs_buffer, dcmprs_size);

        total_dcmprs_size += dcmprs_size;
        rt_kprintf(">");
    }
    rt_kprintf("\n");
    rt_kprintf("decompressed %d bytes into %d bytes !\n", file_size, total_dcmprs_size);

_dcmprs_exit:
    if (dcmprs_buffer)
    {
        free(dcmprs_buffer);
    }

    if(buffer)
    {
        free(buffer);
    }

    return ret;
}

int minilzo_test(int argc, char ** argv)
{
    int fd_in = -1 , fd_out = -1;
    int ret  = 0;

    if (argc != 4)
    {
        rt_kprintf("Usage:\n");
        rt_kprintf("minilzo_test -c [file] [cmprs_file]          -compress \"file\" to \"cmprs_file\" \n");
        rt_kprintf("minilzo_test -d [cmprs_file] [dcmprs_file]   -dcompress \"cmprs_file\" to \"dcmprs_file\" \n");
        
        ret = -1;
        goto _exit;
    }

    rt_kprintf("\nminiLZO real-time data compression library (v%s, %s).\n",
           lzo_version_string(), lzo_version_date());

    /*
    * Initialize the LZO library
    */
    if (lzo_init() != LZO_E_OK)
    {
        rt_kprintf("internal error - lzo_init() failed !!!\n");        
        return 3;
    }    

    fd_in = open(argv[2], O_RDONLY, 0);
    if (fd_in < 0)
    {
        rt_kprintf("[minilzo] open the input file : %s error!\n", argv[2]);
        ret = -1;
        goto _exit;
    }

    fd_out = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC, 0);
    if (fd_out < 0)
    {
        rt_kprintf("[minilzo] open the output file : %s error!\n", argv[3]);
        ret = -1;
        goto _exit;
    }

    if(memcmp("-c", argv[1], strlen(argv[1])) == 0)
    {

        if(minilzo_compress_file(fd_in, fd_out) < 0)
        {
            rt_kprintf("[minilzo] minilzo compress file error!\n");
        }

    }
    else if(memcmp("-d", argv[1], strlen(argv[1])) == 0)
    {

        if(minilzo_decompress_file(fd_in, fd_out) < 0)
        {
            rt_kprintf("[minilzo] minilzo decompress file error!\n");
        }
    }
    else
    {
        rt_kprintf("Usage:\n");
        rt_kprintf("minilzo_test -c [file] [cmprs_file]          -compress \"file\" to \"cmprs_file\" \n");
        rt_kprintf("minilzo_test -d [cmprs_file] [dcmprs_file]   -dcompress \"cmprs_file\" to \"dcmprs_file\" \n");
        
        ret = -1;
        goto _exit;
    }

_exit:
    if(fd_in >= 0)
    {
        close(fd_in);
    }

    if(fd_out >= 0)
    {
        close(fd_out);
    }

    return ret;
}

#ifdef RT_USING_FINSH
#ifdef FINSH_USING_MSH

#include <finsh.h>

MSH_CMD_EXPORT(minilzo_test, minilzo compress and decompress test);
#endif
#endif
