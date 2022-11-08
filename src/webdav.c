/*  webdav.c: send requests to the WebDAV server.
    Copyright (C) 2006, 2007, 2008, 2009, 2014 Werner Baumann

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
#ifdef HAVE_ICONV_H
#include <iconv.h>
#endif
#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif
#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
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
#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <ne_alloc.h>
#include <ne_auth.h>
#include <ne_basic.h>
#include <ne_compress.h>
#include <ne_dates.h>
#include <ne_locks.h>
#include <ne_props.h>
#include <ne_redirect.h>
#include <ne_request.h>
#include <ne_session.h>
#include <ne_socket.h>
#include <ne_string.h>
#include <ne_uri.h>
#include <ne_utils.h>
#include <ne_xml.h>

#include "util.h"
#include "defaults.h"
#include "mount_davfs.h"
#include "webdav.h"

#ifdef ENABLE_NLS
#define _(String) gettext(String)
#else
#define _(String) String
#endif

#include "rpmatch.h"

/* Data Types */
/*============*/

/* Data structures used as userdata in neon callback functions. */

typedef struct {
    const char *path;           /* The *not* url-encoded path. */
    dav_props *results;         /* Start of the linked list of dav_props. */
} propfind_context;

typedef struct {
    int error;                  /* An error occured while reading/writing. */
    const char *file;           /* cache_file to store the data in. */
    int fd;                     /* file descriptor of the open cache file. */
} get_context;

typedef struct {
    int error;
    uint64_t available;         /* Amount of available bytes (quota - used). */
    uint64_t used;              /* Used bytes. */
} quota_context;


/* Private constants */
/*===================*/

/* Properties to be retrieved from the server. This constants
   are used by dav_get_collection(). */
enum {
    TYPE = 0,
    LENGTH,
    ETAG,
    MODIFIED,
    EXECUTE
};

static const ne_propname full_prop_names[] = {
    [TYPE] = {"DAV:", "resourcetype"},
    [LENGTH] = {"DAV:", "getcontentlength"},
    [ETAG] = {"DAV:", "getetag"},
    [MODIFIED] = {"DAV:", "getlastmodified"},
    [EXECUTE] = {"http://apache.org/dav/props/", "executable"},
    {NULL, NULL}
};

static const ne_propname full_anonymous_prop_names[] = {
    [TYPE] = {NULL, "resourcetype"},
    [LENGTH] = {NULL, "getcontentlength"},
    [ETAG] = {NULL, "getetag"},
    [MODIFIED] = {NULL, "getlastmodified"},
    [EXECUTE] = {NULL, "executable"},
    {NULL, NULL}
};

static const ne_propname min_prop_names[] = {
    [TYPE] = {"DAV:", "resourcetype"},
    [LENGTH] = {"DAV:", "getcontentlength"},
    {NULL, NULL}
};

static const ne_propname min_anonymous_prop_names[] = {
    [TYPE] = {NULL, "resourcetype"},
    [LENGTH] = {NULL, "getcontentlength"},
    {NULL, NULL}
};

/* Properties to be retrieved from the server. This constants
   are used by dav_quota(). */
enum {
    AVAILABLE = 0,
    USED
};

static const ne_propname quota_names[] = {
    [AVAILABLE] = {"DAV:", "quota-available-bytes"},
    [USED] = {"DAV:", "quota-used-bytes"},
    {NULL, NULL}
};

static size_t log_bufsize = 512;

static char *none_match_header = "If-None-Match: *\r\n";


/* Private global variables */
/*==========================*/

/* The neon session.
   Will be set by dav_init_webdav(). */
static ne_session *session;

/* Timeout for requests. Set by dav_init_webdav(). */
static int read_timeout;

/* Timeout for creating a connection. Set by dav_init_webdav(). */
static int connect_timeout;

/* Lock store, lock owner and lock timeout for session.
   Will be set by dav_init_webdav(). */
static ne_lock_store *locks;
static char *owner;
static time_t lock_timeout;

/* Credentials for this session. Will be set by dav_init_webdav(). */
static char *username;
static char *password;
static char *p_username;
static char *p_password;

/* If this is not NULL the server must present exactly this certificate. */
static ne_ssl_certificate *server_cert;

/* Whether to send expect 100-continue header in PUT requests. */
static int use_expect100;

/* Whether to use HEAD for prechecking instead of If-Match and If-None-Match
   on PTU and LOCK. */
static int has_if_match_bug;

/* Some servers sends a weak invalid etag that turns into a valid strong etag
   after one second. With this flag set weak etags are not used at all,
   otherwise the weakness indicator is removed. */
static int drop_weak_etags;

/* Check with HEAD for existence or modification of a file, before locking or
   uploading a new file. */
static int precheck;

/* Ignore the information in the DAV-header because it is wrong. */
static int ignore_dav_header;

/* Use "Content-Encoding: gzip" for GET requests. */
static int use_compression;

/* Only request a minimal set of properties. */
static int min_propset;
static const ne_propname *prop_names;
static const ne_propname *anonymous_prop_names;

/* Will be set to 1 when dav_init_connection() succeeded. */
static int initialized;

/* Whether a terminal is available to communicate with the user.
   Should be reset with set_no_terminal() when forking into daemon mode.
   Needed by  ssl_verify() which may be called at any time. */
static int have_terminal;

#ifdef HAVE_ICONV
/* Handle to convert character encoding from utf-8 to LC_CTYPE.
   If NULL no conversion is done. */
static iconv_t from_utf_8;

/* Handle to convert character encoding from LC_CTYPE to utf-8.
   If NULL no conversion is done. */
static iconv_t to_utf_8;

/* Handle to convert character encing of path names to LC_CTYPE. If NULL
   no conversion is done. */
static iconv_t from_server_enc;

/* Handle to convert from LC_CTYPE to character encoding of path names.
   If NULL no conversion is done. */
static iconv_t to_server_enc;
#endif

/* A GNU custom stream, used to redirect neon debug messages to syslog. */
static FILE *log_stream;

/* A user defined header that is added to all requests. */
char *custom_header;

/* Array of max. n_cookies cookie strings. NULL if configuration option
   allow_cookies is 0. The array must be allocated by dav_init_webdav. */
static int n_cookies;
static char **cookie_list;


/* Private function prototypes and inline functions */
/*==================================================*/

#ifdef HAVE_ICONV
static void
convert(char **s, iconv_t conv);
#endif

static ne_session *
create_rd_session(const ne_uri *uri);

static int
get_error(int ret, const char *method, ne_session *sess);

static int
get_ne_error(const char *method, ne_session *sess);

int
get_redirect(const ne_uri *uri, const char *cache_path, off_t *size,
             char **etag, time_t *mtime, int *modified);

static struct ne_lock *
lock_by_path(const char *path);

static int
lock_discover(const char *path, time_t *expire);

static void
lock_refresh(struct ne_lock *lock, time_t *expire);

static ssize_t
log_writer(void *cookie, const char *buffer, size_t size);

static char *
normalize_etag(const char *etag);

static void
replace_slashes(char **name);

/* Call-back functions for neon. */

static void
add_header(ne_request *req, void *userdata, ne_buffer *header);

static int
auth(void *userdata, const char *realm, int attempt, char *user, char *pwd);

static int
file_reader(void *userdata, const char *block, size_t length);

static void
get_cookies(ne_request *req, void *userdata, const ne_status *status);

static void
lock_result(void *userdata, const struct ne_lock *lock, const ne_uri *uri,
            const ne_status *status);

static void
prop_result(void *userdata, const ne_uri *uri, const ne_prop_result_set *set);

static void
quota_result(void *userdata, const ne_uri *uri, const ne_prop_result_set *set);

static int
ssl_verify(void *userdata, int failures, const ne_ssl_certificate *cert);


/* Public functions */
/*==================*/

void
dav_init_webdav(const dav_args *args)
{
    if (args->neon_debug & ~NE_DBG_HTTPPLAIN)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "Initializing webdav");

#ifdef HAVE_ICONV
    char *lc_charset = nl_langinfo(CODESET);
    if (lc_charset && strcasecmp(lc_charset, "UTF-8") != 0) {
        from_utf_8 = iconv_open(lc_charset, "UTF-8");
        if (from_utf_8 == (iconv_t) -1)
            from_utf_8 = 0;
        to_utf_8 = iconv_open("UTF-8", lc_charset);
        if (to_utf_8 == (iconv_t) -1)
            to_utf_8 = 0;
    }

    if (lc_charset && args->s_charset
            && strcasecmp(args->s_charset, lc_charset) != 0) {
        if (strcasecmp(args->s_charset, "UTF-8") == 0) {
            from_server_enc = from_utf_8;
            to_server_enc = to_utf_8;
        } else {
            from_server_enc = iconv_open(lc_charset, args->s_charset);
            if (from_server_enc == (iconv_t) -1)
                from_server_enc = 0;
            to_server_enc = iconv_open(args->s_charset, lc_charset);
            if (to_server_enc == (iconv_t) -1)
                to_server_enc = 0;
        }
    }
#endif /* HAVE_ICONV */

    if (ne_sock_init() != 0)
        ERR(_("socket library initialization failed"));

    if (args->neon_debug & ~NE_DBG_HTTPPLAIN) {
        char *buf = malloc(log_bufsize);
        cookie_io_functions_t *log_func = malloc(sizeof(cookie_io_functions_t));
        if (!log_func) abort();
        log_func->read = NULL;
        log_func->write = log_writer;
        log_func->seek = NULL;
        log_func->close = NULL;
        log_stream = fopencookie(buf, "w", *log_func);
        if (!log_stream)
            ERR(_("can't open stream to log neon-messages"));
        ne_debug_init(log_stream, args->neon_debug);
    }

    session = ne_session_create(args->scheme, args->host, args->port);

    read_timeout = args->read_timeout;
    ne_set_read_timeout(session, read_timeout);
    connect_timeout = args->connect_timeout;
    ne_set_connect_timeout(session, connect_timeout);

    char *useragent = ne_concat(PACKAGE_TARNAME, "/", PACKAGE_VERSION, NULL);
    ne_set_useragent(session, useragent);
    free(useragent);

    if (args->follow_redirect)
        ne_redirect_register(session);

#if NE_VERSION_MAJOR > 0 || NE_VERSION_MINOR > 30
    if (args->sharepoint_href_bug)
        ne_set_session_flag(session, NE_SESSFLAG_SHAREPOINT, 1);
#endif

    if (args->username)
        username = ne_strdup(args->username);
    if (args->password)
        password = ne_strdup(args->password);
    ne_add_server_auth(session, NE_AUTH_ALL, auth, "server");

    if (args->useproxy && args->p_host) {
        ne_session_proxy(session, args->p_host, args->p_port);
        if (args->p_user)
            p_username = ne_strdup(args->p_user);
        if (args->p_passwd)
            p_password = ne_strdup(args->p_passwd);
        ne_add_proxy_auth(session, NE_AUTH_ALL, auth, "proxy");
    }


    if (strcmp(args->scheme, "https") == 0) {
        if (!ne_has_support(NE_FEATURE_SSL))
            ERR(_("neon library does not support TLS/SSL"));

        ne_ssl_set_verify(session, ssl_verify, NULL);
        if (args->trust_server_cert) {
            server_cert = ne_ssl_cert_read(args->trust_server_cert);
            if (!server_cert)
                ERR(_("can't read server certificate %s"),
                      args->trust_server_cert);
        } else if (args->trust_ca_cert) {
            ne_ssl_certificate *ca_cert = ne_ssl_cert_read(args->trust_ca_cert);
            if (!ca_cert)
                ERR(_("can't read server certificate %s"),
                      args->trust_ca_cert);
            ne_ssl_trust_cert(session, ca_cert);
        } else {
            ne_ssl_trust_default_ca(session);
        }

        if (args->clicert) {
            ne_ssl_client_cert *client_cert
                    = ne_ssl_clicert_read(args->clicert);
            if (!client_cert) {
                uid_t orig = geteuid();
                if (seteuid(0) != 0)
                    ERR(_("can't change effective user id"));
                client_cert = ne_ssl_clicert_read(args->clicert);
                if (seteuid(orig) != 0)
                    ERR(_("can't change effective user id"));
            }
            if (!client_cert)
                ERR(_("can't read client certificate %s"),
                      args->clicert);
            if (client_cert && ne_ssl_clicert_encrypted(client_cert)) {
                char *pw = NULL;
                if (!args->clicert_pw && args->askauth) {
                    printf(_("Please enter the password to decrypt client\n"
                             "certificate %s.\n"), args->clicert);
                    pw = dav_user_input_hidden(_("Password: "));
                } else {
                    pw = ne_strdup(args->clicert_pw);
                }
                int ret = 1;
                if (pw) {
                    ret = ne_ssl_clicert_decrypt(client_cert, pw);
                    memset(pw, '\0', strlen(pw));
                    free(pw);
                }
                if (ret)
                    ERR(_("can't decrypt client certificate %s"),
                          args->clicert);
            }
            ne_ssl_set_clicert(session, client_cert);
            ne_ssl_clicert_free(client_cert);
        }
    }

    have_terminal = args->askauth;

    if (args->locks) {
        locks = ne_lockstore_create();
        if (!args->lock_owner) {
            if (!args->username) {
                owner = ne_strdup(PACKAGE_STRING);
            } else {
                owner = ne_strdup(args->username);
            }
        } else {
            owner = ne_strdup(args->lock_owner);
        }
        lock_timeout = args->lock_timeout;
    }

    if (args->header) {
        custom_header = ne_strdup(args->header);
    }

    if (args->n_cookies) {
        n_cookies = args->n_cookies;
        cookie_list = (char **) ne_calloc(n_cookies * sizeof(char *));
        ne_hook_post_headers(session, get_cookies, NULL);
    }

    if (custom_header || cookie_list)
        ne_hook_pre_send(session, add_header, NULL);

    use_expect100 = args->expect100;
    has_if_match_bug = args->if_match_bug;
    drop_weak_etags = args->drop_weak_etags;
    precheck = args->precheck;
    ignore_dav_header = args->ignore_dav_header;
    use_compression = args->use_compression & ne_has_support(NE_FEATURE_ZLIB);
    min_propset = args->min_propset;
    if (min_propset) {
        prop_names = min_prop_names;
        anonymous_prop_names = min_anonymous_prop_names;
    } else {
        prop_names = full_prop_names;
        anonymous_prop_names = full_anonymous_prop_names;
    }
}


int
dav_init_connection(const char *path)
{
    char *spath = ne_path_escape(path);
    ne_server_capabilities caps = {0, 0, 0};
    int ret = ne_options(session, spath, &caps);

    if (!ret) {
        initialized = 1;
        if (!caps.dav_class1 && !ignore_dav_header) {
            if (have_terminal) {
                ERR(_("mounting failed; the server does not support WebDAV"));
            } else {
                syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR),
                       _("mounting failed; the server does not support WebDAV"));
                ret = EINVAL;
            }
        }
        if ((caps.dav_class2 || ignore_dav_header) && locks) {
            ne_lockstore_register(locks, session);
        } else if (locks) {
            if (have_terminal)
                WARN(_("warning: the server does not support locks"));
            ne_lockstore_destroy(locks);
            locks = NULL;
        }
    } else {
        ret = get_error(ret, "OPTIONS", session);
    }

    free(spath);
    return ret;
}


void
dav_close_webdav(void)
{
    if(session && log_stream)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "Closing connection to  %s",
               ne_get_server_hostport(session));

    if (locks) {
        struct ne_lock *lock = ne_lockstore_first(locks);
        while (lock) {
            if (session) {
                int ret = ne_unlock(session, lock);
                get_error(ret, "UNLOCK", session);
            }
            lock = ne_lockstore_next(locks);
        }
    }

    if (session)
        ne_session_destroy(session);
    ne_sock_exit();
}


char *
dav_conv_from_utf_8(const char *s)
{
    char *new = ne_strdup(s);
#ifdef HAVE_ICONV
    if (from_utf_8)
        convert(&new, from_utf_8);
#endif
    return new;
}


char *
dav_conv_to_utf_8(const char *s)
{
    char *new = ne_strdup(s);
#ifdef HAVE_ICONV
    if (to_utf_8)
        convert(&new, to_utf_8);
#endif
    return new;
}


char *
dav_conv_from_server_enc(const char *s)
{
    char *new = ne_strdup(s);
#ifdef HAVE_ICONV
    if (from_server_enc)
        convert(&new, from_server_enc);
#endif
    return new;
}


char *
dav_conv_to_server_enc(const char *s)
{
    char *new = ne_strdup(s);
#ifdef HAVE_ICONV
    if (to_server_enc)
        convert(&new, to_server_enc);
#endif
    return new;
}


int
dav_delete(const char *path, time_t *expire)
{
    int ret;
    if (!initialized) {
        ret = dav_init_connection(path);
        if (ret) return ret;
    }

    struct ne_lock *lock = NULL;
    char *spath = ne_path_escape(path);
    ret = ne_delete(session, spath);
    ret = get_error(ret, "DELETE", session);

    if ((ret == EACCES || ret == ENOENT) && locks) {
        lock_discover(spath, expire);
        lock = lock_by_path(spath);
        if (lock && ret == EACCES) {
            ret = ne_delete(session, spath);
            ret = get_error(ret, "DELETE", session);
        } else if (lock) {
            ne_unlock(session, lock);
            ret = 0;
        }
    }

    if (!ret && locks) {
        lock = lock_by_path(spath);
        if (lock) {
            ne_lockstore_remove(locks, lock);
            ne_lock_destroy(lock);
        }
    }

    free(spath);
    return ret;
}


int
dav_delete_dir(const char *path)
{
    int ret;
    if (!initialized) {
        ret = dav_init_connection(path);
        if (ret) return ret;
    }

    char *spath = ne_path_escape(path);
    ret = ne_delete(session, spath);
    ret = get_error(ret, "DELETE", session);

    free(spath);
    return ret;
}


void
dav_delete_props(dav_props *props)
{
    if (props->path)
        free(props->path);
    if (props->name)
        free(props->name);
    if (props->etag)
        free(props->etag);
    free(props);
}


int
dav_get_collection(const char *path, dav_props **props)
{
    int ret;
    if (!initialized) {
        ret = dav_init_connection(path);
        if (ret) return ret;
    }

    propfind_context ctx;
    ctx.path = path;
    ctx.results = NULL;

    char *spath = ne_path_escape(path);
    ne_propfind_handler *ph = ne_propfind_create(session, spath, NE_DEPTH_ONE);
    ret = ne_propfind_named(ph, prop_names, prop_result, &ctx);
    ret = get_error(ret, "PROPFIND", session);
    ne_propfind_destroy(ph);
    free(spath);

    if (ret) {
        while(ctx.results) {
            dav_props *tofree = ctx.results;
            ctx.results = ctx.results->next;
            dav_delete_props(tofree);
        }
    }

    *props = ctx.results;
    return ret;
}


const char *
dav_get_webdav_error()
{
    return ne_get_error(session);
}


int
dav_get_file(const char *path, const char *cache_path, off_t *size,
             char **etag, time_t *mtime, int *modified)
{
    int ret;
    if (!initialized) {
        ret = dav_init_connection(path);
        if (ret) return ret;
    }

    get_context ctx;
    ctx.error = 0;
    ctx.file = cache_path;
    ctx.fd = 0;

    char *spath = ne_path_escape(path);
    ne_request *req = ne_request_create(session, "GET", spath);

    if (etag && *etag)
        ne_add_request_header(req, "If-None-Match", *etag);
    char *mod_time = NULL;
    if (mtime && *mtime) {
        mod_time = ne_rfc1123_date(*mtime);
        ne_add_request_header(req, "If-Modified-Since", mod_time);
    }

    ne_decompress *dc_state = NULL;
    if (use_compression) {
        dc_state = ne_decompress_reader(req, ne_accept_2xx, file_reader, &ctx);
    } else {
        ne_add_response_body_reader(req, ne_accept_2xx, file_reader, &ctx);
    }

    ret = ne_request_dispatch(req);

    if (use_compression)
        ne_decompress_destroy(dc_state);

    ne_session *rd_sess = NULL;
    const ne_uri *rd_uri = NULL;
    if (ret == NE_REDIRECT)
        rd_uri = ne_redirect_location(session);

    if (rd_uri) {
        rd_sess = create_rd_session(rd_uri);
        char *rd_path = NULL;
        if (rd_uri->query) {
            rd_path = ne_concat(rd_uri->path, "?", rd_uri->query, NULL);
        } else {
            rd_path = ne_strdup(rd_uri->path);
        }

        ne_request_destroy(req);
        req = ne_request_create(rd_sess, "GET", rd_path);

        if (etag && *etag)
            ne_add_request_header(req, "If-None-Match", *etag);

        if (use_compression) {
            dc_state = ne_decompress_reader(req, ne_accept_2xx, file_reader,
                                            &ctx);
        } else {
            ne_add_response_body_reader(req, ne_accept_2xx, file_reader, &ctx);
        }

        ret = ne_request_dispatch(req);

        if (use_compression)
            ne_decompress_destroy(dc_state);
        ret = get_error(ret, "GET", rd_sess);
        free(rd_path);
    } else {
        ret = get_error(ret, "GET", session);
    }

    if (ctx.error)
        ret = ctx.error;
    const ne_status *status = ne_get_status(req);

    if (!ret && status->code == 200) {

        if (modified)
            *modified = 1;

        const char *value = ne_get_response_header(req, "Content-Length");
        if (value)
#if _FILE_OFFSET_BITS == 64
            *size = strtoll(value, NULL, 10);
#else /* _FILE_OFFSET_BITS != 64 */
            *size = strtol(value, NULL, 10);
#endif /* _FILE_OFFSET_BITS != 64 */

        value = ne_get_response_header(req, "Last-Modified");
        if (mtime && value)
            *mtime = ne_httpdate_parse(value);

        if (etag) {
            if (*etag) free(*etag);
            *etag = normalize_etag(ne_get_response_header(req, "ETag"));
        }

    }

    ne_request_destroy(req);
    if (rd_sess)
        ne_session_destroy(rd_sess);
    if (ctx.fd > 0)
        close(ctx.fd);
    if (mod_time)
        free(mod_time);
    free(spath);
    return ret;
}


int
dav_head(const char *path, char **etag, time_t *mtime, off_t *length)
{
    int ret;
    if (!initialized) {
        ret = dav_init_connection(path);
        if (ret) return ret;
    }

    char *spath = ne_path_escape(path);
    ne_request *req = ne_request_create(session, "HEAD", spath);
    ret = ne_request_dispatch(req);
    ret = get_error(ret, "HEAD", session);

    const char *value = ne_get_response_header(req, "ETag");
    if (!ret && etag && value) {
        if (*etag) free(*etag);
        *etag = normalize_etag(value);
    }

    value = ne_get_response_header(req, "Last-Modified");
    if (!ret && mtime && value) {
        time_t lm = ne_httpdate_parse(value);
        if (lm)
            *mtime = lm;
    }

    value = ne_get_response_header(req, "Content-Length");
    if (!ret && length && value)
        *length = strtol(value, NULL, 10);

    ne_request_destroy(req);
    free(spath);
    return ret;
}


int
dav_lock(const char *path, time_t *expire, int *exists)
{
    int ret;
    if (!initialized) {
        ret = dav_init_connection(path);
        if (ret) return ret;
    }

    if (!locks) {
        *expire = 0;
        return 0;
    }

    if (precheck && has_if_match_bug && !*exists) {
        if (dav_head(path, NULL, NULL, NULL) == 0) {
            return EEXIST;
        }
    }

    char *spath = ne_path_escape(path);

    struct ne_lock *lock = lock_by_path(spath);
    if (lock) {
        free(spath);
        *expire = -1;
        lock_refresh(lock, expire);
        return 0;
    }

    lock = ne_lock_create();
    ne_fill_server_uri(session, &lock->uri);
    lock->uri.path = spath;
    lock->owner = ne_strdup(owner);
    lock->timeout = lock_timeout;

    if (!has_if_match_bug && !*exists)
        ne_hook_pre_send(session, add_header, none_match_header);
    ret = ne_lock(session, lock);
    ret = get_error(ret, "LOCK", session);
    if (!has_if_match_bug && !*exists) {
        ne_unhook_pre_send(session, add_header, none_match_header);
        if (ret && strtol(ne_get_error(session), NULL, 10) == 412)
            ret = EEXIST;
    }

    if (!ret) {
        ne_lockstore_add(locks, lock);
        if (strtol(ne_get_error(session), NULL, 10) == 201)
            *exists = 1;
        if (lock->timeout <= 0) {
            *expire = LONG_MAX;
        } else {
            *expire = lock->timeout + time(NULL);
        }
    } else {
        if (ret == EACCES && lock_discover(spath, expire) == 0)
            ret = 0;
        ne_lock_destroy(lock);
    }

    return ret;
}


void
dav_lock_refresh(const char *path, time_t *expire)
{
    if (!initialized && dav_init_connection(path) != 0)
        return;

    if (!locks) {
        *expire = 0;
        return;
    }

    char *spath = ne_path_escape(path);
    struct ne_lock *lock = lock_by_path(spath);

    if (!lock) {
        lock_discover(spath, expire);
    } else {
        lock_refresh(lock, expire);
    }

    free(spath);
}


int
dav_make_collection(const char *path)
{
    int ret;
    if (!initialized) {
        ret = dav_init_connection(path);
        if (ret) return ret;
    }

    char *spath = ne_path_escape(path);
    ret = ne_mkcol(session, spath);
    ret = get_error(ret, "MKCOL", session);

    free(spath);
    return ret;
}


int
dav_move(const char *src, const char *dst)
{
    int ret;
    if (!initialized) {
        ret = dav_init_connection(src);
        if (ret) return ret;
    }

    char *spath = ne_path_escape(src);
    char *dst_path = ne_path_escape(dst);
    ret = ne_move(session, 1, spath, dst_path);
    ret = get_error(ret, "MOVE", session);

    if (!ret && locks) {
        struct ne_lock *lock = lock_by_path(spath);
        if (lock) {
            ne_lockstore_remove(locks, lock);
            ne_lock_destroy(lock);
        }
    }

    free(spath);
    free(dst_path);
    return ret;
}


int
dav_put(const char *path, const char *cache_path, int *exists, time_t *expire,
        char **etag, time_t *mtime, int execute)
{
    int ret = 0;
    if (!initialized) {
        ret = dav_init_connection(path);
        if (ret) return ret;
    }


    if (precheck && (has_if_match_bug || (*exists && (!etag || !*etag)))
            && (!*exists || (etag && *etag) || (mtime && *mtime))) {
        char *r_etag = NULL;
        time_t r_mtime = 0;
        off_t r_length = 0;
        ret = dav_head(path, &r_etag, &r_mtime, &r_length);
        if (!ret) {
            if (!*exists && r_length) {
                ret = EEXIST;
            } else if (etag && *etag && r_etag) {
                if (strcmp(*etag, r_etag) != 0)
                    ret = EINVAL;
            } else if (mtime && *mtime && r_mtime) {
                if (*mtime < r_mtime)
                    ret = EINVAL;
            }
        } else if (ret == ENOENT) {
            if (!*exists) {
                ret = 0;
            } else {
                ret = dav_unlock(path, expire);
            }
        }
        if (r_etag)
            free(r_etag);
        if (ret)
            return ret;
    }

    int fd = open(cache_path, O_RDONLY);
    if (fd <= 0)
        return EIO;
    struct stat st;
    if (fstat(fd, &st) != 0)
        return EIO;

    char *spath = ne_path_escape(path);
    ne_request *req = ne_request_create(session, "PUT", spath);

    if (!has_if_match_bug) {
        if (!*exists) {
            ne_add_request_header(req, "If-None-Match", "*");
        } else {
            if (etag && *etag)
                ne_add_request_header(req, "If-Match", *etag);
        }
    }

    if (use_expect100)
        ne_set_request_flag(req, NE_REQFLAG_EXPECT100, 1);

    ne_lock_using_resource(req, spath, 0);
    ne_set_request_body_fd(req, fd, 0, st.st_size);

    ret = ne_request_dispatch(req);
    ret = get_error(ret, "PUT", session);

    if (ret == EACCES && lock_discover(spath, expire) == 0) {

        ne_request_destroy(req);
        req = ne_request_create(session, "PUT", spath);

        if (!has_if_match_bug) {
            if (!*exists) {
                ne_add_request_header(req, "If-None-Match", "*");
            } else {
                if (etag && *etag)
                    ne_add_request_header(req, "If-Match", *etag);
            }
        }

        if (use_expect100)
            ne_set_request_flag(req, NE_REQFLAG_EXPECT100, 1);

        ne_lock_using_resource(req, spath, 0);
        ne_set_request_body_fd(req, fd, 0, st.st_size);

        ret = ne_request_dispatch(req);
        ret = get_error(ret, "PUT", session);
    }

    int need_head = 0;
    if (!ret) {

        *exists = 1;
        const char *value;

        if (etag) {
            if (*etag) free(*etag);
            *etag = normalize_etag(ne_get_response_header(req, "ETag"));
            if (!*etag)
                need_head = 1;
        }

        if (mtime) {
            value = ne_get_response_header(req, "Last-Modified");
            if (!value)
                value = ne_get_response_header(req, "Date");
            if (value) {
                *mtime = ne_httpdate_parse(value);
            } else {
                need_head = 1;
            }
        }

    }

    ne_request_destroy(req);
    free(spath);
    close(fd);

    if (!ret) {
        if (execute == 1)
            dav_set_execute(path, execute);
        if (need_head)
            dav_head(path, etag, mtime, NULL);
    }

    return ret;
}


int
dav_quota(const char *path, uint64_t *available, uint64_t *used)
{
    int ret;
    if (!initialized) {
        ret = dav_init_connection(path);
        if (ret) return ret;
    }

    static int use_rfc = 1;

    quota_context ctx;
    ctx.error = 0;
    ctx.available = 0;
    ctx.used = 0;
    ret = EIO;
    char *spath = ne_path_escape(path);

    if (use_rfc) {
        ne_propfind_handler *ph = ne_propfind_create(session, spath,
                                                     NE_DEPTH_ZERO);
        ret = ne_propfind_named(ph, quota_names, quota_result, &ctx);
        ret = get_error(ret, "PROPFIND", session);
        ne_propfind_destroy(ph);

        if (!ret && ctx.error) {
            ret = EIO;
            if (ctx.error == 2)
                use_rfc = 0;
        }
    }

    if (!ret) {
        *available = ctx.available;
        *used = ctx.used;
    }

    free(spath);
    return ret;
}


int
dav_set_execute(const char *path, int set)
{
    int ret;
    if (!initialized) {
        ret = dav_init_connection(path);
        if (ret) return ret;
    }

    ne_proppatch_operation op[2];
    op[0].name = &prop_names[EXECUTE];
    op[0].type = ne_propset;
    if (set) {
        op[0].value = "T";
    } else {
        op[0].value = "F";
    }
    op[1].name = NULL;

    char *spath = ne_path_escape(path);
    ret = ne_proppatch(session, spath, &op[0]);
    ret = get_error(ret, "PROPPATCH", session);
    free(spath);

    return ret;
}


void
dav_set_no_terminal(void)
{
    have_terminal = 0;
}


int
dav_unlock(const char *path, time_t *expire)
{
    int ret;
    if (!initialized) {
        ret = dav_init_connection(path);
        if (ret) return ret;
    }

    if (!locks) {
        *expire = 0;
        return 0;
    }

    char *spath = ne_path_escape(path);
    struct ne_lock *lock = lock_by_path(spath);
    free(spath);

    if (!lock) {
        *expire = 0;
        return 0;
    }

    ret = ne_unlock(session, lock);
    ret = get_error(ret, "UNLOCK", session);

    if (!ret || ret == ENOENT || ret == EINVAL) {
        ne_lockstore_remove(locks, lock);
        ne_lock_destroy(lock);
        *expire = 0;
        return 0;
    }

    return ret;
}



/* Private functions */
/*===================*/

#ifdef HAVE_ICONV
static void
convert(char **s, iconv_t conv)
{
    size_t insize = strlen(*s);
    char *in = *s;
    size_t outsize = MB_LEN_MAX * (insize + 1);
    char *buf = calloc(outsize, 1);
    if (!buf) abort();
    char *out = buf;

    iconv(conv, NULL, NULL, &out, &outsize);
    if (iconv(conv, &in, &insize, &out, &outsize) >= 0
            && insize == 0 && outsize >= MB_LEN_MAX) {
        memset(out, 0, MB_LEN_MAX);
        free(*s);
        *s = ne_strndup(buf, out - buf + MB_LEN_MAX);
    }

    free(buf);
}
#endif /* HAVE_ICONV */


/* Creates a new session.
   Scheme, host and port are taken from uri.
   read_timout, connect_timeout, useragent and proxy are the same as for the
   main sessions. The values are taken from the main session or from global
   variables.
   The same call back functions for auth and custom headers as for the main
   session are registered, using the same credentials as the main session.
   If the scheme is https the ssl_verify function is registered. It is set up
   to trust in the default CAs. A server certificate or client certificate
   configured by the user are not used because it may be a different server.
   TODO: usage of certificates and credentials should depend on uri->host.
   Return value : the newly created session. It must be destroyed by the
                  caller. */
static ne_session *
create_rd_session(const ne_uri *uri)
{
    if (!uri->scheme || !uri->host || !uri->path)
        return NULL;
    if (strcmp(uri->scheme, "https") == 0 && !ne_has_support(NE_FEATURE_SSL))
        return NULL;

    int port = uri->port;
    if (port == 0)
        port = ne_uri_defaultport(uri->scheme);

    ne_session *rd_sess = ne_session_create(uri->scheme, uri->host, port);

    ne_set_read_timeout(rd_sess, read_timeout);
    ne_set_connect_timeout(rd_sess, connect_timeout);

    char *useragent = ne_concat(PACKAGE_TARNAME, "/", PACKAGE_VERSION, NULL);
    ne_set_useragent(session, useragent);
    free(useragent);

    ne_add_server_auth(rd_sess, NE_AUTH_ALL, auth, "server");

    ne_uri *proxy = (ne_uri *) ne_calloc(sizeof(ne_uri));
    ne_fill_proxy_uri(rd_sess, proxy);
    if (proxy->host) {
        ne_session_proxy(rd_sess, proxy->host, proxy->port);
        ne_add_proxy_auth(rd_sess, NE_AUTH_ALL, auth, "proxy");
    }

    if (strcmp(uri->scheme, "https") == 0) {
        ne_ssl_set_verify(rd_sess, ssl_verify, NULL);
        ne_ssl_trust_default_ca(rd_sess);
    }

    if (custom_header)
        ne_hook_pre_send(rd_sess, add_header, custom_header);

    return rd_sess;
}


/* Returns a file error code according to ret from the last WebDAV
   method call. If ret has value NE_ERROR the error code from the session is
   fetched and translated.
   ret    : the error code returned from NEON.
   method : name of the WebDAV method, used for debug messages.
   sess   : the session to get the HTTP status code from.
   return value : a file error code according to errno.h. */
static int
get_error(int ret, const char *method, ne_session *sess)
{
    int err;
    switch (ret) {
    case NE_OK:
    case NE_ERROR:
        err = get_ne_error(method, sess);
        break;
    case NE_LOOKUP:
        err = EIO;
        break;
    case NE_AUTH:
        err = EPERM;
        break;
    case NE_PROXYAUTH:
        err = EPERM;
        break;
    case NE_CONNECT:
        err = EAGAIN;
        break;
    case NE_TIMEOUT:
        err = EAGAIN;
        break;
    case NE_FAILED:
        err = EINVAL;
        break;
    case NE_RETRY:
        err = EAGAIN;
        break;
    case NE_REDIRECT:
        err = ENOENT;
        break;
    default:
        err = EIO;
        break;
    }

    return err;
}


/* Get the error from the session and translates it into a file error code.
   method : name of the WebDAV method, used for debug messages.
   sess   : the session to get the HTTP status code from.
   return value : a file error code according to errno.h. */
static int
get_ne_error(const char *method, ne_session *sess)
{
    const char *text = ne_get_error(sess);

    char *tail;
    int err = strtol(text, &tail, 10);
    if (tail == text)
        return EIO;

    switch (err) {
        case 200:           /* OK */
        case 201:           /* Created */
        case 202:           /* Accepted */
        case 203:           /* Non-Authoritative Information */
        case 204:           /* No Content */
        case 205:           /* Reset Content */
        case 207:           /* Multi-Status */
        case 304:           /* Not Modified */
            return 0;
        case 401:           /* Unauthorized */
        case 402:           /* Payment Required */
        case 407:           /* Proxy Authentication Required */
            return EPERM;
        case 301:           /* Moved Permanently */
        case 303:           /* See Other */
        case 404:           /* Not Found */
        case 405:           /* Method Not Allowed */
        case 410:           /* Gone */
            return ENOENT;
        case 408:           /* Request Timeout */
        case 504:           /* Gateway Timeout */
            return EAGAIN;
        case 423:           /* Locked */
            return EACCES;
        case 400:           /* Bad Request */
        case 403:           /* Forbidden */
        case 409:           /* Conflict */
        case 411:           /* Length Required */
        case 412:           /* Precondition Failed */
        case 414:           /* Request-URI Too Long */
        case 415:           /* Unsupported Media Type */
        case 424:           /* Failed Dependency */
        case 501:           /* Not Implemented */
            return EINVAL;
        case 413:           /* Request Entity Too Large */
        case 507:           /* Insufficient Storage */
            return ENOSPC;
        case 206:           /* Partial Content */
        case 300:           /* Multiple Choices */
        case 302:           /* Found */
        case 305:           /* Use Proxy */
        case 306:           /* (Unused) */
        case 307:           /* Temporary Redirect */
        case 406:           /* Not Acceptable */
        case 416:           /* Requested Range Not Satisfiable */
        case 417:           /* Expectation Failed */
        case 422:           /* Unprocessable Entity */
        case 500:           /* Internal Server Error */
        case 502:           /* Bad Gateway */
        case 503:           /* Service Unavailable */
        case 505:           /* HTTP Version Not Supported */
            return EIO;
        default:
            return EIO;
    }
}


/* Searches for a lock for resource at path and returns
   this lock if successfull, NULL otherwise. */
static struct ne_lock *
lock_by_path(const char *path)
{
    struct ne_lock *lock = ne_lockstore_first(locks);
    while (lock) {
        if (strcmp(path, lock->uri.path) == 0)
            return lock;
        lock = ne_lockstore_next(locks);
    }

    return NULL;
}


/* Checks if there is already a lock for file path that is owned by owner. If
   successful it stores the lock in the global lock store, refreshes the lock
   and updates expire.
   If no matching lock is found or the session is initialized with the nolocks
   option, it  sets expire to 0 and returns -1.
   If a matching lock is found, but it can not be refreshed, expire is set
   to -1 (= locked, expire time unknown).
   If an error occurs it leaves expire unchanged and returns -1:
   path   : URL-escaped path of the file on the server.
   expire : The time when the lock expires, will be updated.
   return value : 0 if a matching lock is found, -1 otherwise. */
static int
lock_discover(const char *path, time_t *expire)
{
    if (!locks) {
        *expire = 0;
        return -1;
    }

    struct ne_lock *lock = NULL;
    int ret = ne_lock_discover(session, path, lock_result, &lock);
    ret = get_error(ret, "LOCKDISCOVER", session);

    if (!ret && lock)  {
        *expire = -1;
        lock_refresh(lock, expire);
        return 0;
    } else {
        if (!ret)
            *expire = 0;
        return -1;
    }
}


/* Refreshes lock and updates expire.
   If an error occurs it does nothing.
   lock   : The lock, will be updated on success.
   expire : The time when the lock expires, updated on success. */
static void
lock_refresh(struct ne_lock *lock, time_t *expire)
{
    int ret = ne_lock_refresh(session, lock);
    ret = get_error(ret, "LOCKREFRESH", session);

    if (!ret) {
        if (lock->timeout <= 0) {
            *expire = LONG_MAX;
        } else {
            *expire = lock->timeout + time(NULL);
        }
    }
}


static ssize_t
log_writer(void *cookie, const char *buffer, size_t size)
{
    if (size <= 0)
        return 0;

    size_t written = 0;
    const char *bpos = buffer;
    while (written < size) {
        size_t n = 2;
        char *cpos = (char *) cookie;
        while (n < log_bufsize && written < size) {
            if (*bpos == '%') {
                *cpos++ = '%';
                n++;
            }
            *cpos++ = *bpos++;
            n++;
            written++;
        }
        *cpos = '\0';
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), "%s", (char *) cookie);
    }
    return written;
}


/* Checks etag for weakness indicator and quotation marks.
   The reurn value is either a strong etag with quotation marks or NULL.
   Depending on global variable drop_weak_etags weak etags are either
   dropped or convertet into strong ones. */
static char *
normalize_etag(const char *etag)
{
    if (!etag) return NULL;

    const char * e = etag;
    if (*e == 'W') {
        if (drop_weak_etags) {
            return NULL;
        } else {
            e++;
            if (*e == '/') {
                e++;
            } else {
                return NULL;
            }
        }
    }
    if (!*e) return NULL;

    char *ne = NULL;
    if (*e == '\"') {
        ne = strdup(e);
    } else {
        if (asprintf(&ne, "\"%s\"", e) < 0)
            ne = NULL;;
    }

    return ne;
}


/* Replaces slashes in name by "slash-", "-slash-" or "-slash", depending
   on the position of the slash within name. */
static void
replace_slashes(char **name)
{
    char *slash = strchr(*name, '/');
    while (slash) {
        char *end = *name + strlen(*name) - 1;
        char *nn;
        *slash = '\0';
        if (slash == *name) {
            nn = ne_concat("slash-", slash + 1, NULL);
        } else if (slash == end) {
            nn = ne_concat(*name, "-slash", NULL);
        } else {
            nn = ne_concat(*name, "-slash-", slash + 1, NULL);
        }
        free(*name);
        *name = nn;
        slash = strchr(*name, '/');
    }
}


/* Call-back functions for neon. */

static void
add_header(ne_request *req, void *userdata, ne_buffer *header)
{
    if (custom_header)
        ne_buffer_zappend(header, custom_header);

    if (cookie_list && cookie_list[0]) {
        ne_buffer_zappend(header, "Cookie: ");
        ne_buffer_zappend(header, cookie_list[0]);
        int i = 1;
        while (i < n_cookies && cookie_list[i]) {
            ne_buffer_zappend(header, "; ");
            ne_buffer_zappend(header, cookie_list[i]);
            i++;
        }
        ne_buffer_zappend(header, "\r\n");
    }
}


/* Copies credentials from global variables into user and pwd.
   userdata must be a string with value "server" or "proxy", to decide what
   the creditentials are needed for.
   The creditentials are taken form global variables username/password or
   p_username/p_password.
   If attempt > 0, this is logged as an error and the value of attempt is
   returned, so neon will not try again.
   userdata : What the credentials are needed for ("server" or "proxy").
   realm    : Used for error log.
   attempt  : Number of attempts to get credentials. If not 0 an error occured.
   user     : A buffer of size NE_ABUFSIZ to return the username.
   pwd      : A buffer of size NE_ABUFSIZ to return the password.
   return value : value if attempt. neon will not call this function again if
                  it is greater than 0. */
static int
auth(void *userdata, const char *realm, int attempt, char *user, char *pwd)
{
    if (attempt) {
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR), _("authentication failure:"));
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR), "  %s", realm);
        return attempt;
    }

    if (strcmp((char *) userdata, "server") == 0) {
        if (username)
            strncpy(user, username, NE_ABUFSIZ - 1);
        if (password)
            strncpy(pwd, password, NE_ABUFSIZ - 1);
    } else if (strcmp((char *) userdata, "proxy") == 0) {
        if (p_username)
            strncpy(user, p_username, NE_ABUFSIZ - 1);
        if (p_password)
            strncpy(pwd, p_password, NE_ABUFSIZ - 1);
    }

    return 0;
}


/* Reads HTTP-data from blockand writes them to a local file.
   userdata must be a get_context structure that holds at least the name of
   the local file. If it does not contain a file descriptor, the file is
   opened for writing and the file descriptor is stored in the get_context
   structure. In case of an error a error flag is set.
   userdata : A get_context structure, containing the name of the local file,
              the file descriptor (if the file is open), and an error flag.
   block    : Buffer containing the data.
   length   : Number of bytes in the buffer.
   return value : 0 on success, EIO otherwise. */
static int
file_reader(void *userdata, const char *block, size_t length)
{
    get_context *ctx = (get_context *) userdata;
    if (!ctx->fd)
        ctx->fd = open(ctx->file, O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
    if (ctx->fd <= 0) {
        ne_set_error(session, _("%i can't open cache file"), 0);
        ctx->error = EIO;
    }

    while (!ctx->error && length > 0) {
        ssize_t ret = write(ctx->fd, block, length);
        if (ret < 0) {
            ctx->error = EIO;
            ne_set_error(session, _("%i error writing to cache file"), 0);
        } else {
            length -= ret;
            block += ret;
        }
    }

    return ctx->error;
}


/* A ne_post_header_fn to read cookies from the Set-Cookie header and
   store them in the global arrray of strings cookie_list. The cookies
   will be send in the Cookie header of subsequent requests.
   Only the "name=value" part of the cookie is stored. All attributes
   are completely ignored.
   When a cookie with the same name as an already stored cookie, but with
   a different value is received, it's value is updated if necessary.
   Only n_cookies cookies will be stored. If the server sends more
   different cookies these will be ignored. */
static void
get_cookies(ne_request *req, void *userdata, const ne_status *status)
{
    void *cursor = NULL;
    const char *name;
    const char *value;

    cursor = ne_response_header_iterate(req, cursor, &name, &value);
    while (cursor) {
        if (strcasecmp(name, "Set-Cookie") != 0) {
            cursor = ne_response_header_iterate(req, cursor, &name, &value);
            continue;
        }

        char *s;
        const char *end = strchr(value, ';');
        if (end) {
            s = ne_strndup(value, end - value);
        } else {
            s = ne_strdup(value);
        }
        char *es = strchr(s, '=');
        if (!es) {
            free(s);
            cursor = ne_response_header_iterate(req, cursor, &name, &value);
            continue;
        }

        char *cookie = s;
        while (*cookie == ' ')
            cookie++;
        while (*(cookie + strlen(cookie) - 1) == ' ')
            *(cookie + strlen(cookie) - 1) = 0;
        int nl = es - cookie;

        int i = 0;
        for (i = 0; i < n_cookies; i++) {
            if (!cookie_list[i]) {
                cookie_list[i] = ne_strdup(cookie);
                break;
            }
            if (strncmp(cookie_list[i], cookie, nl) == 0) {
                free(cookie_list[i]);
                cookie_list[i] = ne_strdup(cookie);
                break;
            }
        }

        free(s);
        cursor = ne_response_header_iterate(req, cursor, &name, &value);
    }
}


/* If the owner of this lock is the same as global variable owner, lock is
   stored in the global lock store locks and a pointer to the lock is
   returned in userdata.
   Otherwise it does nothing.
   userdata : *userdata will be set to lock, if lock is ownded ba owner.
   lock     : a lock found by ne_lock_discover() on the server.
   uri      : not used.
   status   : not used. */
static void
lock_result(void *userdata, const struct ne_lock *lock, const ne_uri *uri,
            const ne_status *status)
{
    if (!locks || !owner || !userdata || !lock || !lock->owner)
        return;

    if (strcmp(lock->owner, owner) == 0) {
        struct ne_lock *l = ne_lock_copy(lock);
        ne_lockstore_add(locks, l);
        l->timeout = lock_timeout;
        *((struct ne_lock **) userdata) = l;
    }
}


/* Called by ne_propfind_named(). Evaluates a dav_props structure from
   href/uri and set and stores it in userdata.
   userdata must be a pointer to a propfind_context structure. Its member
   path holds the unescaped path of the collection. Its member results is
   a linked list of the dav_props structures.
   The unescaped version of href/uri->path must must be equal to the path of
   the collection or a descendent of it. It is stored as member path of the
   dav_props structure. It will be normalized (collections have a trailing
   slash, non-collections do not have one).
   The name is derived from path. The name of the
   collection itself will be the empty string. If name contains
   a '/'-character it is replaced by the ugly string "-slash-".
   There must not be two dav_props structure with the same path or the same
   name. in this case one of them is removed from the list, preferable the one
   that is not a directory or that is less specific.
   userdata : A pointer to a propfind_context structure containing the path of
              the collection and the linked list of properties.
   uri      : ne_uri of the resource as returned from the server.
   set      : Points to the set of properties returned from the server.*/
static void
prop_result(void *userdata, const ne_uri *uri, const ne_prop_result_set *set)
{
    propfind_context *ctx = (propfind_context *) userdata;
    if (!ctx || !uri || !uri->path || !set)
        return;

    char *tmp_path = (char *) ne_malloc(strlen(uri->path) + 1);
    const char *from = uri->path;
    char *to = tmp_path;
    while (*from) {
        while (*from == '/' && *(from + 1) == '/')
            from++;
        *to++ = *from++;
    }
    *to = 0;
    dav_props *result = ne_calloc(sizeof(dav_props));
    result->path = ne_path_unescape(tmp_path);
    free (tmp_path);

    if (!result->path || strlen(result->path) < 1) {
        dav_delete_props(result);
        return;
    }

    const char *data;

    data = ne_propset_value(set, &prop_names[TYPE]);
    if (!data)
        data = ne_propset_value(set, &anonymous_prop_names[TYPE]);
    if (data && strstr(data, "collection"))
            result->is_dir = 1;

    if (*(result->path + strlen(result->path) - 1) == '/') {
        if (!result->is_dir)
            *(result->path + strlen(result->path) - 1) = '\0';
    } else {
        if (result->is_dir) {
            char *tmp = ne_concat(result->path, "/", NULL);
            free(result->path);
            result->path = tmp;
        }
    }

    if (strcasestr(result->path, ctx->path) != result->path) {
        dav_delete_props(result);
        return;
    }

    if (strcasecmp(result->path, ctx->path) == 0) {
        result->name = ne_strdup("");
    } else {
        if (strlen(result->path) < (strlen(ctx->path) + result->is_dir + 1)) {
            dav_delete_props(result);
            return;
        }
        result->name = ne_strndup(result->path + strlen(ctx->path),
                                  strlen(result->path) - strlen(ctx->path)
                                  - result->is_dir);
        replace_slashes(&result->name);
#ifdef HAVE_ICONV
        if (from_server_enc)
            convert(&result->name, from_server_enc);
#endif
    }

    data = ne_propset_value(set, &prop_names[LENGTH]);
    if (!data)
        data = ne_propset_value(set, &anonymous_prop_names[LENGTH]);
    if (data)
#if _FILE_OFFSET_BITS == 64
         result->size = strtoll(data, NULL, 10);
#else /* _FILE_OFFSET_BITS != 64 */
         result->size = strtol(data, NULL, 10);
#endif /* _FILE_OFFSET_BITS != 64 */

    if (!min_propset) {
        data = ne_propset_value(set, &prop_names[ETAG]);
        if (!data)
            data = ne_propset_value(set, &anonymous_prop_names[ETAG]);
        result->etag = normalize_etag(data);

        data = ne_propset_value(set, &prop_names[MODIFIED]);
        if (!data)
            data = ne_propset_value(set, &anonymous_prop_names[MODIFIED]);
        if (data) {
            result->mtime = ne_httpdate_parse(data);
            if (result->mtime == (time_t) -1)
                result->mtime = ne_iso8601_parse(data);
            if (result->mtime == (time_t) -1)
                result->mtime = 0;
        }

        data = ne_propset_value(set, &prop_names[EXECUTE]);
        if (!data)
            data = ne_propset_value(set, &anonymous_prop_names[EXECUTE]);
        if (!data) {
            result->is_exec = -1;
        } else if (*data == 'T') {
            result->is_exec = 1;
        }
    }

    result->next = ctx->results;
    ctx->results = result;
}


/* Reads available and used bytes from set and stores them in
   userdata. */
static void
quota_result(void *userdata, const ne_uri *uri, const ne_prop_result_set *set)
{
    quota_context *ctx = (quota_context *) userdata;
    if (!ctx  || !uri || !uri->path || !set)
        return;

    const char *data = ne_propset_value(set, &quota_names[AVAILABLE]);
    if (data) {
        ctx->available = strtoull(data, NULL, 10);
    } else {
        const ne_status *st = ne_propset_status(set, &quota_names[AVAILABLE]);
        if (st && st->klass == 4) {
            ctx->error = 2;
        } else {
            ctx->error = 1;
        }
        return;
    }

    data = ne_propset_value(set, &quota_names[USED]);
    if (data)
        ctx->used = strtoull(data, NULL, 10);
}


/* Displays information about cert and asks the user whether to accept
   the certificate or not.
   If no terminal is available (according to global variable Have_terminal)
   it returns an error. Else it displays an error message and certificate
   date and ask whether to accept the certificate. If the user accepts
   it returns 0, otherwise an error.
   In any case the event is logged.
   userdata : not used.
   failures : a constant indicating the kind of error.
   cert     : the server certificate that could not be verified by neon.
   return value : 0 accept the certificate for this session.
                  -1 don't accept the certificate. */
static int
ssl_verify(void *userdata, int failures, const ne_ssl_certificate *cert)
{
    if (server_cert) {
        if (ne_ssl_cert_cmp(cert, server_cert) == 0)
            return 0;
        if (have_terminal)
            WARN(_("the server certificate is not trusted"));
        return -1;
    }

    char *issuer = ne_ssl_readable_dname(ne_ssl_cert_issuer(cert));
    char *subject = ne_ssl_readable_dname(ne_ssl_cert_subject(cert));
    char *digest = ne_calloc(NE_SSL_DIGESTLEN);
    if (!issuer || !subject || ne_ssl_cert_digest(cert, digest) != 0) {
        if (have_terminal) {
            WARN(_("error processing server certificate"));
        } else {
            syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR),
                   _("error processing server certificate"));
        }
        if (issuer) free(issuer);
        if (subject) free(subject);
        if (digest) free(digest);
        return -1;
    }

    int ret = -1;
    if (have_terminal) {
        if (failures & NE_SSL_NOTYETVALID)
            WARN(_("the server certificate is not yet valid"));
        if (failures & NE_SSL_EXPIRED)
            WARN(_("the server certificate has expired"));
        if (failures & NE_SSL_IDMISMATCH)
            WARN(_("the server certificate does not match the server name"));
        if (failures & NE_SSL_UNTRUSTED)
            WARN(_("the server certificate is not trusted"));
        if (failures & ~NE_SSL_FAILMASK)
            WARN(_("unknown certificate error"));
        printf(_("  issuer:      %s"), issuer);
        printf("\n");
        printf(_("  subject:     %s"), subject);
        printf("\n");
        printf(_("  identity:    %s"), ne_ssl_cert_identity(cert));
        printf("\n");
        printf(_("  fingerprint: %s"), digest);
        printf("\n");
            printf(_("You only should accept this certificate, if you can\n"
                     "verify the fingerprint! The server might be faked\n"
                     "or there might be a man-in-the-middle-attack.\n"));
            printf(_("Accept certificate for this session? [y,N] "));
            char *s = NULL;
            size_t n = 0;
            ssize_t len = 0;
            len = getline(&s, &n, stdin);
            if (len < 0)
                abort();
            if (rpmatch(s) > 0)
                ret = 0;
            free(s);
    }

    if (failures & NE_SSL_NOTYETVALID)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR),
               _("the server certificate is not yet valid"));
    if (failures & NE_SSL_EXPIRED)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR),
               _("the server certificate has expired"));
    if (failures & NE_SSL_IDMISMATCH)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR),
               _("the server certificate does not match the server name"));
    if (failures & NE_SSL_UNTRUSTED)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR),
               _("the server certificate is not trusted"));
    if (failures & ~NE_SSL_FAILMASK)
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR),
               _("unknown certificate error"));
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR), _("  issuer: %s"), issuer);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR), _("  subject: %s"), subject);
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR), _("  identity: %s"),
                       ne_ssl_cert_identity(cert));
    if (!ret) {
        syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR), _("  accepted by user"));
    }

    if (issuer) free(issuer);
    if (subject) free(subject);
    if (digest) free(digest);
    return ret;
}

