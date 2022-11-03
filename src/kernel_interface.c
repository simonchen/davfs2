/*  kernel_interface.c.
    Copyright (C) 2006, 2007, 2008, 2009, 2014, 2020 Werner Baumann
    Copyright (C) 2022  Ali Abdallah <ali.abdallah@suse.com>

    This file is part of davfs2.

    davfs2 is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    davfs2 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with davfs2; if not, write to the Free Software Foundation,
    Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA. */


#include "config.h"

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#include <sys/uio.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <sys/wait.h>

#include "util.h"
#include "defaults.h"
#include "mount_davfs.h"
#include "cache.h"
#include "fuse_kernel.h"
#include "kernel_interface.h"

#ifdef ENABLE_NLS
#define _(String) gettext(String)
#else
#define _(String) String
#endif


/* Private constants */
/*===================*/

/* Name of the device to communicate with the kernel file system. */
#define FUSE_DEV_NAME "fuse"

#ifdef __FreeBSD__
enum { DAV_IOV_FSTYPE, DAV_IOV_ALLOW_OTHER, DAV_IOV_FSPATH, DAV_IOV_FSNAME,
       DAV_IOV_FD, DAV_IOV_MAX_READ, DAV_IOV_ERR, DAV_IOV_MAX};

/* max pairs of iovec mount options */
#define DAV_MAX_IOVEC_LEN (DAV_IOV_MAX + 1) * 2

/* Code taken from FreeBSD sbin/mount */
static void
add_iovec_opt(struct iovec **iov, int *iovlen, const char *name, void *val,
        size_t len)
{
    int i;

    if (*iovlen < 0)
        return;

    i = *iovlen;

    if (i + 2 > (DAV_MAX_IOVEC_LEN))
    {
        ERR("DAV_MAX_IOVEC_LEN reached\n");
    }

    (*iov)[i].iov_base = strdup(name);
    (*iov)[i].iov_len = strlen(name) + 1;

    i++;

    (*iov)[i].iov_base = val;

    if (len == (size_t)-1) {
        if (val != NULL)
            len = strlen(val) + 1;
        else
            len = 0;
    }
    (*iov)[i].iov_len = (int)len;
    *iovlen = ++i;
}

static void free_iovec (struct iovec *iov, int iovlen)
{
    int i = 0;
    for (i = 0; i <= iovlen;  i+=2)
    {
        if (iov[i].iov_base)
            free(iov[i].iov_base);
    }

    free(iov);
}
#endif

/* Public functions */
/*==================*/

void
dav_init_kernel_interface(int *dev, size_t *buf_size, const char *url,
                          const char *mpoint, const dav_args *args)
{
#ifdef __FreeBSD__
    struct iovec *iov;
    int iovlen;
    char errmsg[255];
    char fdstr[15];
    char bufstr[15];

    iovlen = 0;
    iov = NULL;
    memset(errmsg, 0, sizeof(errmsg));
#endif

    uid_t orig = geteuid();
    if (seteuid(0) != 0)
        ERR(_("can't change effective user id"));

    char *path;
    if (asprintf(&path, "%s/%s", DAV_DEV_DIR, FUSE_DEV_NAME) < 0)
            abort();

    *dev = open(path, O_RDWR | O_NONBLOCK);

    if (*dev <= 0) {
        ERR(_("loading kernel module fuse"));
        int ret;
        pid_t pid = fork();
        if (pid == 0) {
#if defined(__FreeBSD__)
            execl("/sbin/kldload", "kldload", "fusefs", NULL);
#elif defined(__linux__)
            execl("/sbin/modprobe", "modprobe", "fuse", NULL);
#endif
            _exit(EXIT_FAILURE);
        } else if (pid < 0) {
            exit(EXIT_FAILURE);
        } else {
            if (waitpid(pid, &ret, 0) != pid)
                exit(EXIT_FAILURE);
        }

        if (ret) {
            WARN(_("loading kernel module fuse failed"));
        } else {
            *dev = open(path, O_RDWR | O_NONBLOCK);
        }

        if (*dev <= 0) {
            WARN(_("waiting for /dev/fuse to be created"));
            sleep(2); 
            *dev = open(path, O_RDWR | O_NONBLOCK);
        }
    }

    free(path);
    if (*dev <= 0) {
        ERR(_("can't open fuse device"));
    }

    if (*buf_size < (FUSE_MIN_READ_BUFFER + 4096)) {
        *buf_size = FUSE_MIN_READ_BUFFER + 4096;
    }

#if defined(__FreeBSD__)
    sprintf(fdstr, "%d", *dev);
    sprintf(bufstr, "%lu", (unsigned long int) (*buf_size - 4096));

    int allow_other = 1;

    iov = malloc (sizeof (struct iovec) * (DAV_MAX_IOVEC_LEN));

    add_iovec_opt(&iov, &iovlen, "fstype", __DECONST(void *, "fusefs"), (size_t)-1);
    add_iovec_opt(&iov, &iovlen, "allow_other", __DECONST(void *, &allow_other), (size_t)-1);
    add_iovec_opt(&iov, &iovlen, "fspath", __DECONST(void *, mpoint), (size_t)-1);
    add_iovec_opt(&iov, &iovlen, "from", __DECONST(void *, "/dev/fuse"), (size_t)-1);

    add_iovec_opt(&iov, &iovlen, "fsname=", __DECONST(void *, url), strlen(url)+1);
    add_iovec_opt(&iov, &iovlen, "fd", fdstr, (size_t)-1);
    add_iovec_opt(&iov, &iovlen, "max_read=", bufstr, (ssize_t)-1);
    add_iovec_opt(&iov, &iovlen, "errmsg", errmsg, sizeof(errmsg));

    if (nmount(iov, iovlen, args->mopts) == -1) {
        if (*errmsg != '\0')
            err(EXIT_FAILURE, _("mounting failed %s : %s"), url, errmsg);
        else
            err(EXIT_FAILURE, _("mounting failed %s"), url);
    }

    free_iovec(iov, iovlen);

#elif defined(__linux__)
    char *mdata;
    if (asprintf(&mdata, "fd=%i,rootmode=%o,user_id=%i,group_id=%i,"
                 "allow_other,max_read=%lu", *dev, args->dir_mode, args->uid,
                 args->gid, (unsigned long int) (*buf_size - 4096)) < 0)
        abort();
    if (mount(url, mpoint, "fuse", args->mopts, mdata) != 0) {
        ERR(_("mounting failed"));
    }

    free(mdata);
#endif
    if (seteuid(orig) != 0)
        ERR(_("can't change effective user id"));
}
