
/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#ifndef NSPV_RPC_H
#define NSPV_RPC_H

char* NSPV_externalip = "127.0.0.1";
char* htmlfiles[] = {"/index", "/bootstrap.min.css", "/bootstrap.min.css.map", "/custom.css", "/favicon.ico", "/font/rubik.css", "/antara150x150.png", "/images/antara150x150.png", "/images/sub-header-logo-min.png", "/font/iJWHBXyIfDnIV7Eyjmmd8WD07oB-.woff2", "/font/iJWKBXyIfDnIV7nBrXyw023e.woff2", "/font/iJWHBXyIfDnIV7F6iGmd8WD07oB-.woff2"};

char* methodfiles[] = {"wallet", "login", "broadcast", "getinfo", "receive", "getnewaddress", "index", "getpeerinfo", "send_confirm", "send_validate", "send", "txidinfo", "logout"};

/**
 * - we need to include WinSock2.h header to correctly use windows structure
 * as the application is still using 32bit structure from mingw so, we need to
 * add the include based on checking
 * @author - fadedreamz@gmail.com
 * @remarks - #if (defined(_M_X64) || defined(__amd64__)) && defined(WIN32)
 *     is equivalent to #if defined(_M_X64) as _M_X64 is defined for MSVC only
 */
#if defined(_M_X64)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#endif
#ifdef _WIN32
#include <winsock2.h>
#endif

#ifdef _WIN32
#define PTW32_STATIC_LIB
#include "pthread.h"

#ifndef NATIVE_WINDOWS
#define EADDRINUSE WSAEADDRINUSE
#endif

#else
#include <netdb.h>
#include <poll.h>
#include <sys/time.h>
#include <time.h>
#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>
#define closesocket close
#endif

#include <errno.h>

#ifndef _WIN32
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0x4000 // Do not generate SIGPIPE
#endif
#else
#define MSG_NOSIGNAL 0
#endif

static int32_t spawned, maxspawned, rpcdepth;
portable_mutex_t NSPV_commandmutex;
uint32_t NSPV_STOP_RECEIVED, DOCKERFLAG;

char* stats_validmethods[] =
    {
        "help"};

int32_t LP_valid_remotemethod(cJSON* argjson)
{
    char* method;
    int32_t i;
    if (DOCKERFLAG != 0)
        return (1);
    if ((method = jstr(argjson, "method")) != 0) {
        for (i = 0; i < (int32_t)(sizeof(stats_validmethods) / sizeof(*stats_validmethods)); i++)
            if (strcmp(method, stats_validmethods[i]) == 0)
                return (1);
        printf("got invalid method.%s remotely\n", method);
    }
    return (-1);
}

#ifdef _WIN32
#define in6_addr sockaddr
#define in_addr_t struct sockaddr_storage

#ifndef NATIVE_WINDOWS
#define EAFNOSUPPORT WSAEAFNOSUPPORT
#endif

/*struct sockaddr_in6 {
    short   sin6_family;
    u_short sin6_port;
    u_long  sin6_flowinfo;
    struct  in6_addr sin6_addr;
    u_long  sin6_scope_id;
};*/
#else
#endif

#ifdef _WIN32
#ifdef AF_INET6
#undef AF_INET6
#endif
#define AF_INET6 23
#endif
static int inet_ntop4(unsigned char* src, char* dst, size_t size);
static int inet_ntop6(unsigned char* src, char* dst, size_t size);
static int inet_pton4(char* src, unsigned char* dst);
static int inet_pton6(char* src, unsigned char* dst);

int32_t portable_ntop(int af, void* src, char* dst, size_t size)
{
    switch (af) {
    case AF_INET:
        return (inet_ntop4(src, dst, size));
    case AF_INET6:
        return (inet_ntop6(src, dst, size));
    default:
        return -1;
    }
    /* NOTREACHED */
}


static int inet_ntop4(unsigned char* src, char* dst, size_t size)
{
    static const char fmt[] = "%u.%u.%u.%u";
    char tmp[sizeof "255.255.255.255"];
    int l;

#ifndef _WIN32
    l = snprintf(tmp, sizeof(tmp), fmt, src[0], src[1], src[2], src[3]);
#else
    l = _snprintf(tmp, sizeof(tmp), fmt, src[0], src[1], src[2], src[3]);
#endif
    if (l <= 0 || (size_t)l >= size) {
        return -1;
    }
    strncpy(dst, tmp, size);
    dst[size - 1] = '\0';
    return 0;
}

static int inet_ntop6(unsigned char* src, char* dst, size_t size)
{
    /*
     * Note that int32_t and int16_t need only be "at least" large enough
     * to contain a value of the specified size.  On some systems, like
     * Crays, there is no such thing as an integer variable with 16 bits.
     * Keep this in mind if you think this function should have been coded
     * to use pointer overlays.  All the world's not a VAX.
     */
    char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"], *tp;
    struct {
        int base, len;
    } best, cur;
    unsigned int words[sizeof(struct in6_addr) / sizeof(uint16_t)];
    int i;

    /*
     * Preprocess:
     *  Copy the input (bytewise) array into a wordwise array.
     *  Find the longest run of 0x00's in src[] for :: shorthanding.
     */
    memset(words, '\0', sizeof words);
    for (i = 0; i < (int)sizeof(struct in6_addr); i++)
        words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));
    best.base = -1;
    best.len = 0;
    cur.base = -1;
    cur.len = 0;
    for (i = 0; i < (int)(sizeof(struct in6_addr) / sizeof(uint16_t)); i++) {
        if (words[i] == 0) {
            if (cur.base == -1)
                cur.base = i, cur.len = 1;
            else
                cur.len++;
        } else {
            if (cur.base != -1) {
                if (best.base == -1 || cur.len > best.len)
                    best = cur;
                cur.base = -1;
            }
        }
    }
    if (cur.base != -1) {
        if (best.base == -1 || cur.len > best.len)
            best = cur;
    }
    if (best.base != -1 && best.len < 2)
        best.base = -1;

    /*
     * Format the result.
     */
    tp = tmp;
    for (i = 0; i < (int)(sizeof(struct in6_addr) / sizeof(uint16_t)); i++) {
        /* Are we inside the best run of 0x00's? */
        if (best.base != -1 && i >= best.base &&
            i < (best.base + best.len)) {
            if (i == best.base)
                *tp++ = ':';
            continue;
        }
        /* Are we following an initial run of 0x00s or any real hex? */
        if (i != 0)
            *tp++ = ':';
        /* Is this address an encapsulated IPv4? */
        if (i == 6 && best.base == 0 && (best.len == 6 || (best.len == 7 && words[7] != 0x0001) || (best.len == 5 && words[5] == 0xffff))) {
            int err = inet_ntop4(src + 12, tp, sizeof tmp - (tp - tmp));
            if (err)
                return err;
            tp += strlen(tp);
            break;
        }
        tp += sprintf(tp, "%x", words[i]);
    }
    /* Was it a trailing run of 0x00's? */
    if (best.base != -1 && (best.base + best.len) == (sizeof(struct in6_addr) / sizeof(uint16_t)))
        *tp++ = ':';
    *tp++ = '\0';

    /*
     * Check for overflow, copy, and we're done.
     */
    if ((size_t)(tp - tmp) > size) {
        return ENOSPC;
    }
    strcpy(dst, tmp);
    return 0;
}


int portable_pton(int af, char* src, void* dst)
{
    switch (af) {
    case AF_INET:
        return (inet_pton4(src, dst));
    case AF_INET6:
        return (inet_pton6(src, dst));
    default:
        return EAFNOSUPPORT;
    }
    /* NOTREACHED */
}


static int inet_pton4(char* src, unsigned char* dst)
{
    static const char digits[] = "0123456789";
    int saw_digit, octets, ch;
    unsigned char tmp[sizeof(struct in_addr)], *tp;
    char savestr[64];
    strcpy(savestr, src);

    //printf("inet_pton4(%s)\n",src);
    saw_digit = 0;
    octets = 0;
    *(tp = tmp) = 0;
    while ((ch = (uint8_t)*src++) != '\0') {
        char* pch;
        if ((pch = strchr(digits, ch)) != NULL) {
            unsigned int nw = (unsigned int)(*tp * 10 + (pch - digits));
            if (saw_digit && *tp == 0) {
                printf("inet_pton4 0\n");
                return EINVAL;
            }
            if (nw > 255) {
                printf("inet_pton4 1\n");
                return EINVAL;
            }
            *tp = nw;
            if (!saw_digit) {
                if (++octets > 4) {
                    printf("inet_pton4 2\n");
                    return EINVAL;
                }
                saw_digit = 1;
            }
        } else if (ch == '.' && saw_digit) {
            if (octets == 4) {
                printf("inet_pton4 3\n");
                return EINVAL;
            }
            *++tp = 0;
            saw_digit = 0;
        } else {
            printf("inet_pton4 4 error.(%s)\n", savestr); //getchar();
            return EINVAL;
        }
    }
    if (octets < 4) {
        printf("inet_pton4 5 error.(%s)\n", savestr); //getchar();
        return EINVAL;
    }
    memcpy(dst, tmp, sizeof(struct in_addr));
    //printf("not errors %08x\n",*(int32_t *)dst);
    return 0;
}


static int inet_pton6(char* src, unsigned char* dst)
{
    static char xdigits_l[] = "0123456789abcdef",
                xdigits_u[] = "0123456789ABCDEF";
    unsigned char tmp[sizeof(struct in6_addr)], *tp, *endp, *colonp;
    char *xdigits, *curtok;
    int ch, seen_xdigits;
    unsigned int val;

    memset((tp = tmp), '\0', sizeof tmp);
    endp = tp + sizeof tmp;
    colonp = NULL;
    /* Leading :: requires some special handling. */
    if (*src == ':')
        if (*++src != ':')
            return EINVAL;
    curtok = src;
    seen_xdigits = 0;
    val = 0;
    while ((ch = *src++) != '\0' && ch != '%') {
        char* pch;

        if ((pch = strchr((xdigits = xdigits_l), ch)) == NULL)
            pch = strchr((xdigits = xdigits_u), ch);
        if (pch != NULL) {
            val <<= 4;
            val |= (pch - xdigits);
            if (++seen_xdigits > 4)
                return EINVAL;
            continue;
        }
        if (ch == ':') {
            curtok = src;
            if (!seen_xdigits) {
                if (colonp)
                    return EINVAL;
                colonp = tp;
                continue;
            } else if (*src == '\0') {
                return EINVAL;
            }
            if (tp + sizeof(uint16_t) > endp)
                return EINVAL;
            *tp++ = (unsigned char)(val >> 8) & 0xff;
            *tp++ = (unsigned char)val & 0xff;
            seen_xdigits = 0;
            val = 0;
            continue;
        }
        if (ch == '.' && ((tp + sizeof(struct in_addr)) <= endp)) {
            int err;

            /* Scope id present, parse ipv4 addr without it */
            pch = strchr(curtok, '%');
            if (pch != NULL) {
                char tmp2[sizeof "255.255.255.255"];

                memcpy(tmp2, curtok, pch - curtok);
                curtok = tmp2;
                src = pch;
            }

            err = inet_pton4(curtok, tp);
            if (err == 0) {
                tp += sizeof(struct in_addr);
                seen_xdigits = 0;
                break; /*%< '\\0' was seen by inet_pton4(). */
            }
        }
        return EINVAL;
    }
    if (seen_xdigits) {
        if (tp + sizeof(uint16_t) > endp)
            return EINVAL;
        *tp++ = (unsigned char)(val >> 8) & 0xff;
        *tp++ = (unsigned char)val & 0xff;
    }
    if (colonp != NULL) {
        /*
         * Since some memmove()'s erroneously fail to handle
         * overlapping regions, we'll do the shift by hand.
         */
        int n = (int)(tp - colonp);
        int i;

        if (tp == endp)
            return EINVAL;
        for (i = 1; i <= n; i++) {
            endp[-i] = colonp[n - i];
            colonp[n - i] = 0;
        }
        tp = endp;
    }
    if (tp != endp)
        return EINVAL;
    memcpy(dst, tmp, sizeof tmp);
    return 0;
}

uint16_t parse_ipaddr(char* ipaddr, char* ip_port)
{
    int32_t j;
    uint16_t port = 0;
    if (ip_port != 0 && ip_port[0] != 0) {
        strcpy(ipaddr, ip_port);
        for (j = 0; ipaddr[j] != 0 && j < 60; j++)
            if (ipaddr[j] == ':') {
                port = atoi(ipaddr + j + 1);
                break;
            }
        ipaddr[j] = 0;
        //printf("%p.(%s) -> (%s:%d)\n",ip_port,ip_port,ipaddr,port);
    } else
        strcpy(ipaddr, "127.0.0.1");
    return (port);
}

uint64_t _calc_ipbits(char* ip_port)
{
    int32_t port;
    char ipaddr[64];
    struct sockaddr_in addr;
    port = parse_ipaddr(ipaddr, ip_port);
    memset(&addr, 0, sizeof(addr));
    portable_pton(ip_port[0] == '[' ? AF_INET6 : AF_INET, ipaddr, &addr);
    if ((0)) {
        int i;
        for (i = 0; i < 16; i++)
            printf("%02x ", ((uint8_t*)&addr)[i]);
        printf("<- %s %x\n", ip_port, *(uint32_t*)&addr);
    }
    return (*(uint32_t*)&addr | ((uint64_t)port << 32));
}

void expand_ipbits(char* ipaddr, uint64_t ipbits)
{
    uint16_t port;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    *(uint32_t*)&addr = (uint32_t)ipbits;
    portable_ntop(AF_INET, &addr, ipaddr, 64);
    if ((port = (uint16_t)(ipbits >> 32)) != 0)
        sprintf(ipaddr + strlen(ipaddr), ":%d", port);
    //sprintf(ipaddr,"%d.%d.%d.%d",(ipbits>>24)&0xff,(ipbits>>16)&0xff,(ipbits>>8)&0xff,(ipbits&0xff));
}

uint64_t calc_ipbits(char* ip_port)
{
    uint64_t ipbits = 0;
    char ipaddr[64], ipaddr2[64];
    int32_t i;
    if (ip_port != 0) {
        ipbits = _calc_ipbits(ip_port);
        expand_ipbits(ipaddr, ipbits);
        if (ipbits != 0 && strcmp(ipaddr, ip_port) != 0) {
            for (i = 0; i < 63; i++)
                if ((ipaddr[i] = ip_port[i]) == ':' || ipaddr[i] == 0)
                    break;
            ipaddr[i] = 0;
            ipbits = _calc_ipbits(ipaddr);
            expand_ipbits(ipaddr2, ipbits);
            if (ipbits != 0 && strcmp(ipaddr, ipaddr2) != 0) {
                if (ipaddr[0] != 0)
                    printf("calc_ipbits error: (%s) -> %llx -> (%s)\n", ip_port, (long long)ipbits, ipaddr); //, getchar();
                ipbits = 0;
            }
        }
    }
    return (ipbits);
}

char* ipbits_str(char ipaddr[64], uint64_t ipbits)
{
    expand_ipbits(ipaddr, ipbits);
    return (ipaddr);
}

uint32_t is_ipaddr(char* str)
{
    uint64_t ipbits;
    char ipaddr[64];
    if (str != 0 && str[0] != 0 && (ipbits = calc_ipbits(str)) != 0) {
        expand_ipbits(ipaddr, (uint32_t)ipbits);
        if (strncmp(ipaddr, str, strlen(ipaddr)) == 0)
            return ((uint32_t)ipbits);
    }
    // printf("(%s) is not ipaddr\n",str);
    return (0);
}

int32_t Supernet_lineparse(char* key, int32_t keymax, char* value, int32_t valuemax, char* src)
{
    int32_t a, b, c, n = 0; //char *origkey=key,*origvalue=value;
    key[0] = value[0] = 0;
    while ((c = src[n]) == ' ' || c == '\t' || c == '\n' || c == '\t')
        n++;
    while ((c = src[n]) != ':' && c != 0) {
        *key++ = c;
        //printf("(%c) ",c);
        if (++n >= keymax - 1) {
            *key = 0;
            printf("lineparse overflow key.(%s)\n", src);
            return (-1);
        }
    }
    *key = 0;
    //printf("-> key.(%s)\n",origkey);
    if (src[n] != ':')
        return (n);
    n++;
    while ((c = src[n]) == ' ' || c == '\t')
        n++;
    while ((c = src[n]) != 0 && c != '\r' && c != '\n') {
        if (c == '%' && (a = src[n + 1]) != 0 && (b = src[n + 2]) != 0)
            c = ((unhex(a) << 4) | unhex(b)), n += 2;
        *value++ = c;
        n++;
        if (n >= valuemax - 1) {
            *value = 0;
            printf("lineparse overflow.(%s)\n", src);
            return (-1);
        }
    }
    *value = 0;
    if (src[n] != 0) {
        n++;
        while ((c = src[n]) == '\r' || c == '\n')
            n++;
    }
    //printf("key.(%s) value.(%s)\n",origkey,origvalue);
    return (n);
}

cJSON* SuperNET_urlconv(char* value, int32_t bufsize, char* urlstr)
{
    int32_t i, n, totallen, datalen, len = 0;
    cJSON *json, *array;
    char key[8192], *data;
    json = cJSON_CreateObject();
    array = cJSON_CreateArray();
    totallen = (int32_t)strlen(urlstr);
    while (1) {
        for (i = len; urlstr[i] != 0; i++)
            if (urlstr[i] == '\r' || urlstr[i] == '\n')
                break;
        if (i == len && (urlstr[len] == '\r' || urlstr[len] == '\n')) {
            len++;
            continue;
        }
        urlstr[i] = 0;
        //printf("URLSTR[%d]=%s\n",i,&urlstr[len]);
        if ((n = Supernet_lineparse(key, sizeof(key), value, bufsize, &urlstr[len])) > 0) {
            if (value[0] != 0)
                jaddstr(json, key, value);
            else
                jaddistr(array, key);
            len += (n + 1);
            if ((strcmp(key, "Content-Length") == 0 || strcmp(key, "content-length") == 0) && (datalen = atoi(value)) > 0) {
                data = &urlstr[totallen - datalen];
                data[-1] = 0;
                //printf("post.(%s) (%c)\n",data,data[0]);
                jaddstr(json, "POST", data);
            }
        } else
            break;
    }
    jadd(json, "lines", array);
    //printf("urlconv.(%s)\n",jprint(json,0));
    return (json);
}

char* NSPV_rpcparse(int32_t* contentlenp, char* retbuf, int32_t bufsize, int32_t* jsonflagp, int32_t* postflagp, char* urlstr, char* remoteaddr, char* filetype, uint16_t port)
{
    cJSON *tokens, *argjson, *origargjson, *tmpjson = 0, *json = 0;
    long filesize;
    char symbol[64], *userpass = 0, urlmethod[16], *data, url[8192], furl[8192], *retstr = 0, *filestr = 0, *token = 0;
    int32_t i, j, n, apiflag = 0, num = 0;
    uint32_t queueid;
    for (i = 0; i < (int32_t)sizeof(urlmethod) - 1 && urlstr[i] != 0 && urlstr[i] != ' '; i++)
        urlmethod[i] = urlstr[i];
    urlmethod[i++] = 0;
    n = i;
    //printf("URLMETHOD.(%s)\n",urlmethod);
    *postflagp = (strcmp(urlmethod, "POST") == 0);
    //printf("POST.%d rpcparse.(%s)\n",*postflagp,urlstr);
    memset(url, 0, 8192);
    for (i = 0; i < (int32_t)sizeof(url) - 1 && urlstr[n + i] != 0 && urlstr[n + i] != ' '; i++)
        url[i] = urlstr[n + i];
    url[i++] = 0;
    n += i;
    j = i = 0;
    filetype[0] = 0;
    //printf("url.(%s) method.(%s) postflag.%d\n",&url[i],urlmethod,*postflagp);
    snprintf(furl, sizeof(furl), "%s", url + 1);
    if (strncmp(&url[i], "/api", strlen("/api")) == 0) {
        *jsonflagp = 1;
        apiflag = 1;
        i += strlen("/api");
    } else
        *jsonflagp = 0;
    if (strcmp(&url[i], "/") == 0 && strcmp(urlmethod, "GET") == 0) {
        *jsonflagp = 1;
        if ((filestr = OS_filestr(&filesize, "html/index")) == 0)
            return (clonestr("{\"error\":\"cant find index\"}"));
        fprintf(stderr, "got index request %p\n", (void*)filestr);
        argjson = cJSON_CreateObject();
        jaddstr(argjson, "method", "index");
        retstr = NSPV_JSON(argjson, remoteaddr, port, filestr, apiflag);
        free_json(argjson);
        return (retstr);
        //else return(filestr);
    } else {
        int32_t j, f, matches;
        char fname[512], *cmpstr, *cmpstr2;
        cmpstr = clonestr(&url[i]);
        cmpstr2 = malloc(strlen(cmpstr) + 64);
        if (cmpstr[strlen(cmpstr) - 1] == '?')
            cmpstr[strlen(cmpstr) - 1] = 0;
        sprintf(cmpstr2, ":%u%s", port, cmpstr);
        //fprintf(stderr,"cmp.(%s) and cmp2.(%s) port.%u\n",cmpstr,cmpstr2,port);
        for (f = 0; f < (int32_t)(sizeof(htmlfiles) / sizeof(*htmlfiles)); f++) {
            if (strcmp(cmpstr, htmlfiles[f]) == 0 || strcmp(cmpstr2, htmlfiles[f]) == 0) {
                *jsonflagp = 1;
                for (j = (int32_t)strlen(url) - 1; j > 0; j--)
                    if (url[j] == '.' || url[j] == '/')
                        break;
                if (url[j] == '.') {
                    sprintf(fname, "html/%s", htmlfiles[f] + 1);
                    strcpy(filetype, url + j + 1);
                    //printf("set (%s) filetype.(%s)\n",fname,filetype);
                    if ((filestr = OS_filestr(&filesize, fname)) == 0) {
                        free(cmpstr);
                        free(cmpstr2);
                        return (clonestr("{\"error\":\"cant find htmlfile\"}"));
                    }
                    if (strcmp(filetype, "jpg") == 0 || strcmp(filetype, "png") == 0 || strcmp(filetype, "ico") == 0)
                        *contentlenp = (int32_t)filesize;
                    free(cmpstr);
                    free(cmpstr2);
                    return (filestr);
                }
            }
        }
        for (f = 0; f < (int32_t)(sizeof(methodfiles) / sizeof(*methodfiles)); f++) {
            if (strncmp(cmpstr + 1, methodfiles[f], strlen(methodfiles[f])) == 0) {
                *jsonflagp = 1;
                strcpy(filetype, "html");
                sprintf(fname, "html/%s", methodfiles[f]);
                //fprintf(stderr,"open1 (%s)\n",fname);
                if ((filestr = OS_filestr(&filesize, fname)) == 0) {
                    free(cmpstr);
                    free(cmpstr2);
                    return (clonestr("{\"error\":\"cant find methodfile\"}"));
                }
                break;
            }
        }
        if (filestr == 0 && strncmp("/method/", cmpstr, 8) == 0) {
            //fprintf(stderr,"cmpstr[8] (%s)\n",cmpstr+8);
            for (f = 0; f < (int32_t)(sizeof(methodfiles) / sizeof(*methodfiles)); f++) {
                if (strncmp(cmpstr + 8, methodfiles[f], strlen(methodfiles[f])) == 0) {
                    *jsonflagp = 1;
                    strcpy(filetype, "html");
                    sprintf(fname, "html/%s", methodfiles[f]);
                    //fprintf(stderr,"open (%s)\n",fname);
                    if ((filestr = OS_filestr(&filesize, fname)) == 0) {
                        free(cmpstr);
                        free(cmpstr2);
                        return (clonestr("{\"error\":\"cant find methodfile\"}"));
                    }
                    break;
                }
            }
            if (filestr == 0) {
                for (f = 0; f < (int32_t)(sizeof(htmlfiles) / sizeof(*htmlfiles)); f++) {
                    //fprintf(stderr,"cmp.(%s) and cmp2.(%s) port.%u\n",cmpstr,cmpstr2,port);
                    if (strcmp(cmpstr + 7, htmlfiles[f]) == 0 || strcmp(cmpstr2 + 7, htmlfiles[f]) == 0) {
                        *jsonflagp = 1;
                        for (j = (int32_t)strlen(url) - 1; j > 0; j--)
                            if (url[j] == '.' || url[j] == '/')
                                break;
                        if (url[j] == '.') {
                            sprintf(fname, "html/%s", htmlfiles[f] + 1);
                            strcpy(filetype, url + j + 1);
                            //printf("set2 (%s) filetype.(%s)\n",fname,filetype);
                            if ((filestr = OS_filestr(&filesize, fname)) == 0) {
                                free(cmpstr);
                                free(cmpstr2);
                                return (clonestr("{\"error\":\"cant find htmlfile\"}"));
                            }
                            if (strcmp(filetype, "jpg") == 0 || strcmp(filetype, "png") == 0 || strcmp(filetype, "ico") == 0)
                                *contentlenp = (int32_t)filesize;
                            free(cmpstr);
                            free(cmpstr2);
                            return (filestr);
                        }
                    }
                }
            }
        }
        free(cmpstr);
        free(cmpstr2);
    }
    /*else if ( (filestr= OS_filestr(&filesize,furl)) != 0 ) allows arbitrary file access!
     {
     *jsonflagp = 1;
     for (i=(int32_t)strlen(url)-1; i>0; i--)
     if ( url[i] == '.' || url[i] == '/' )
     break;
     if ( url[i] == '.' )
     strcpy(filetype,url+i+1);
     //printf("return filetype.(%s) size.%ld\n",filetype,filesize);
     return(filestr);
     }*/
    if (strcmp(url, "/favicon.ico") == 0) {
        *jsonflagp = 1;
        return (0);
    }
    if (url[i] != '/')
        token = &url[i];
    n = i;
    tokens = cJSON_CreateArray();
    for (; url[i] != 0; i++) {
        //printf("i.%d (%c)\n",i,url[i]);
        if (url[i] == '/') {
            url[i] = 0;
            if (token != 0) {
                //printf("TOKEN.(%s) i.%d\n",token,i);
                jaddistr(tokens, token);
                num++;
            }
            token = &url[i + 1];
            i++;
            //printf("new token.(%s) i.%d\n",token,i+1);
            continue;
        }
    }
    if (token != 0) {
        //printf("add token.(%s)\n",token);
        jaddistr(tokens, token);
        num++;
    }
    argjson = cJSON_CreateObject();
    if (num > 0)
        jaddstr(argjson, "agent", jstri(tokens, 0));
    if (num > 1)
        jaddstr(argjson, "method", jstri(tokens, 1));
    if ((json = SuperNET_urlconv(retbuf, bufsize, urlstr + n)) != 0) {
        jadd(json, "tokens", tokens);
        jaddstr(json, "urlmethod", urlmethod);
        if ((data = jstr(json, "POST")) != 0) {
            free_json(argjson);
            if (strncmp("wif=", data, 4) == 0) {
                argjson = cJSON_CreateObject();
                jaddstr(argjson, "method", "login");
                jaddstr(argjson, "wif", data + 4);
                memset(data, 0, strlen(data));
            } else
                argjson = cJSON_Parse(data);
            //printf("data.(%s) -> (%s)\n",data,jprint(argjson,0));
        }
        if (argjson != 0) {
            char* buf = malloc(NSPV_MAXPACKETSIZE);
            userpass = jstr(argjson, "userpass");
            //printf("userpass.(%s)\n",userpass);
            if ((n = cJSON_GetArraySize(tokens)) > 0) {
                if (n > 1) {
                    if (jstri(tokens, 1) != 0) {
                        char *key, *value;
                        strcpy(buf, jstri(tokens, 1));
                        key = value = 0;
                        i = 0;
                        for (; buf[i] != 0; i++) {
                            if (buf[i] == '?') {
                                buf[i] = 0;
                                jdelete(argjson, "method");
                                jaddstr(argjson, "method", buf);
                                i++;
                                key = &buf[i];
                                break;
                            }
                        }
                        while (buf[i] != 0) {
                            //printf("iter.[%s]\n",&buf[i]);
                            if (buf[i] != 0 && key != 0) {
                                for (; buf[i] != 0; i++) {
                                    if (buf[i] == '=') {
                                        buf[i] = 0;
                                        i++;
                                        //printf("got key.(%s)\n",key);
                                        value = &buf[i];
                                        break;
                                    }
                                }
                                if (buf[i] != 0 && value != 0) {
                                    for (; buf[i] != 0; i++) {
                                        if (buf[i] == '&') {
                                            buf[i] = 0;
                                            jaddstr(argjson, key, value);
                                            i++;
                                            //printf("got value.(%s)\n",value);
                                            value = 0;
                                            key = &buf[i];
                                            break;
                                        } else if (buf[i] == '+')
                                            buf[i] = ' ';
                                    }
                                }
                            }
                        }
                        if (key != 0 && value != 0)
                            jaddstr(argjson, key, value);
                    } else {
                        //jdelete(argjson,"method");
                        //jaddstr(argjson,"method",buf);
                    }
                }
                for (i = 2; i < n; i++) {
                    if (i == n - 1)
                        jaddstr(argjson, "data", jstri(tokens, i));
                    else {
                        if (strcmp(jstri(tokens, i), "coin") == 0 && strlen(jstri(tokens, i + 1)) < sizeof(symbol) - 1) {
                            strcpy(symbol, jstri(tokens, i + 1));
                            touppercase(symbol);
                            jaddstr(argjson, jstri(tokens, i), symbol);
                        } else
                            jaddstr(argjson, jstri(tokens, i), jstri(tokens, i + 1));
                        i++;
                    }
                }
                free(buf);
            }
            if (is_cJSON_Array(argjson) != 0 && (n = cJSON_GetArraySize(argjson)) > 0) {
                cJSON *retitem, *retarray = cJSON_CreateArray();
                origargjson = argjson;
                symbol[0] = 0;
                for (i = 0; i < n; i++) // array cmd path doesnt support event streaming
                {
                    argjson = jitem(origargjson, i);
                    if (userpass != 0 && jstr(argjson, "userpass") == 0)
                        jaddstr(argjson, "userpass", userpass);
                    //printf("after urlconv.(%s) argjson.(%s)\n",jprint(json,0),jprint(argjson,0));
                    if (strcmp(remoteaddr, "127.0.0.1") == 0 || strcmp(remoteaddr, NSPV_externalip) == 0 || LP_valid_remotemethod(argjson) > 0) {
                        if ((retstr = NSPV_JSON(argjson, remoteaddr, port, filestr, apiflag)) != 0) {
                            if ((retitem = cJSON_Parse(retstr)) != 0)
                                jaddi(retarray, retitem);
                            free(retstr);
                        }
                    } else
                        retstr = clonestr("{\"error\":\"invalid remote method\"}");
                    //printf("(%s) {%s} -> (%s) postflag.%d (%s)\n",urlstr,jprint(argjson,0),jprint(json,0),*postflagp,retstr);
                }
                free_json(origargjson);
                retstr = jprint(retarray, 1);
            } else {
                cJSON* arg;
                char *buf, *method;
                if (jstr(argjson, "agent") != 0 && strcmp(jstr(argjson, "agent"), "bitcoinrpc") != 0 && jobj(argjson, "params") != 0) {
                    arg = jobj(argjson, "params");
                    if (is_cJSON_Array(arg) != 0 && cJSON_GetArraySize(arg) == 1)
                        arg = jitem(arg, 0);
                } else
                    arg = argjson;
                //printf("ARGJSON.(%s) filestr.%p\n",jprint(arg,0),filestr);
                if (userpass != 0 && jstr(arg, "userpass") == 0)
                    jaddstr(arg, "userpass", userpass);
                if (strcmp(remoteaddr, "127.0.0.1") == 0 || strcmp(remoteaddr, NSPV_externalip) == 0 || LP_valid_remotemethod(arg) > 0) {
                    portable_mutex_lock(&NSPV_commandmutex);
                    retstr = NSPV_JSON(arg, remoteaddr, port, filestr, apiflag);
                    portable_mutex_unlock(&NSPV_commandmutex);
                } else
                    retstr = clonestr("{\"error\":\"invalid remote method\"}");
            }
            free_json(argjson);
        }
        free_json(json);
        if (tmpjson != 0)
            free(tmpjson);
        return (retstr);
    }
    free_json(argjson);
    if (tmpjson != 0)
        free_json(tmpjson);
    if (tokens != 0)
        free_json(tokens);
    *jsonflagp = 1;
    return (clonestr("{\"error\":\"couldnt process packet\"}"));
}

int32_t iguana_getcontentlen(char* buf) //,int32_t recvlen)
{
    char *str, *clenstr = "Content-Length: ", *clenstr2 = "content-length: ";
    int32_t len = -1;
    if ((str = strstr(buf, clenstr)) != 0 || (str = strstr(buf, clenstr2)) != 0) {
        //printf("strstr.(%s)\n",str);
        str += strlen(clenstr);
        len = atoi(str);
        //printf("len.%d\n",len);
    }
    return (len);
}

int32_t iguana_getheadersize(char* buf, int32_t recvlen)
{
    char *str, *delim = "\r\n\r\n";
    if ((str = strstr(buf, delim)) != 0)
        return ((int32_t)(((long)str - (long)buf) + strlen(delim)));
    return (recvlen);
}

static char space[NSPV_MAXPACKETSIZE], space2[NSPV_MAXPACKETSIZE];

void* LP_rpc_processreq(void* _ptr)
{
    char filetype[128], content_type[128];
    int32_t recvlen, retlen, flag, postflag = 0, contentlen, remains, sock, numsent, jsonflag = 0, hdrsize, len;
    char helpname[512], remoteaddr[64], *buf, *retstr, *jsonbuf;
    struct rpcrequest_info* req = _ptr;
    uint32_t ipbits, i, size = NSPV_MAXPACKETSIZE + 512;
    ipbits = req->ipbits;
    ;
    expand_ipbits(remoteaddr, ipbits);
    sock = req->sock;
    recvlen = flag = 0;
    retstr = 0;
    jsonbuf = calloc(1, size);
    remains = size - 1;
    buf = jsonbuf;
    if (spawned < 0)
        spawned = 0;
    spawned++;
    if (spawned > maxspawned) {
        printf("max rpc threads spawned and alive %d <- %d\n", maxspawned, spawned);
        maxspawned = spawned;
    }
    while (remains > 0) {
        //printf("flag.%d remains.%d recvlen.%d\n",flag,remains,recvlen);
        if ((len = (int32_t)recv(sock, buf, remains, 0)) < 0) {
            if (errno == EAGAIN) {
                printf("EAGAIN for len %d, remains.%d\n", len, remains);
                usleep(10000);
            }
            //printf("errno.%d len.%d remains.%d\n",errno,len,remains);
            break;
        } else {
            //printf("received len.%d\n%s\n",len,buf);
            if (len > 0) {
                buf[len] = 0;
                if (recvlen == 0) {
                    if ((contentlen = iguana_getcontentlen(buf)) > 0) {
                        hdrsize = iguana_getheadersize(buf, recvlen);
                        if (hdrsize > 0) {
                            if (len < (hdrsize + contentlen)) {
                                remains = (hdrsize + contentlen) - len;
                                buf = &buf[len];
                                flag = 1;
                                //printf("got.(%s) %d remains.%d of len.%d contentlen.%d hdrsize.%d remains.%d\n",buf,recvlen,remains,len,contentlen,hdrsize,(hdrsize+contentlen)-len);
                                continue;
                            }
                        }
                    }
                }
                recvlen += len;
                remains -= len;
                buf = &buf[len];
                if (flag == 0 || remains <= 0)
                    break;
            } else {
                usleep(10000);
                printf("got.(%s) %d remains.%d of total.%d\n", jsonbuf, recvlen, remains, len);
                if (flag == 0)
                    break;
            }
        }
    }
    content_type[0] = 0;
    retlen = 0;
    if (recvlen > 0) {
        jsonflag = postflag = 0;
        retstr = NSPV_rpcparse(&retlen, space, size, &jsonflag, &postflag, jsonbuf, remoteaddr, filetype, req->port);
        if (filetype[0] != 0) {
            static cJSON* mimejson;
            char *tmp, *typestr = 0;
            long tmpsize;
            sprintf(helpname, "mime.json");
            if ((tmp = OS_filestr(&tmpsize, helpname)) != 0) {
                mimejson = cJSON_Parse(tmp);
                free(tmp);
            }
            if (mimejson != 0) {
                if ((typestr = jstr(mimejson, filetype)) != 0)
                    sprintf(content_type, "Content-Type: %s\r\n", typestr);
            } else
                printf("parse error.(%s)\n", tmp);
            //printf("filetype.(%s) json.%p type.%p tmp.%p [%s]\n",filetype,mimejson,typestr,tmp,content_type);
        }
    }
    if (retstr != 0) {
        char *response, *acceptstr = "", hdrs[1024] = {0};
        int32_t crflag = 1;
        //printf("RETURN.(%s) jsonflag.%d postflag.%d\n",retstr,jsonflag,postflag);
        if (jsonflag != 0 || postflag != 0) {
            if (strlen(retstr) + 1024 + 1 + 1 < sizeof(space2))
                response = space2;
            else {
                response = malloc(strlen(retstr) + 1024 + 1 + 1);
                //printf("alloc response.%p\n",response);
            }
            if (retlen == 0)
                retlen = (int32_t)strlen(retstr) + 1;
            else {
                acceptstr = "Accept-Ranges: bytes\r\n";
                crflag = 0;
            }
            sprintf(hdrs, "HTTP/1.1 200 OK\r\n%sAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Credentials: true\r\nAccess-Control-Allow-Methods: GET, POST\r\nContent-Security-Policy: default-src 'self'; style-src 'self' custom.css bootstrap.min.css 'unsafe-inline'; connect-src 'none'; object-src 'none'; frame-src 'none'; child-src 'none'\r\nCache-Control :  no-cache, no-store, must-revalidate\r\n%sContent-Length : %8d\r\n\r\n", acceptstr, content_type, retlen);
            response[0] = '\0';
            strcat(response, hdrs);
            memcpy(&response[strlen(response)], retstr, retlen);
            if (crflag != 0)
                strcat(response, "\n");
            if (retstr != space) {
                //printf("free retstr0.%p\n",retstr);
                free(retstr);
            }
            retstr = response;
            //printf("RET.(%s)\n",retstr);
        }
        remains = (int32_t)strlen(hdrs) + retlen;
        i = 0;
        while (remains > 0) {
            if ((numsent = (int32_t)send(sock, &retstr[i], remains, MSG_NOSIGNAL)) < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    //printf("%s: %s numsent.%d vs remains.%d len.%d errno.%d (%s) usock.%d\n",retstr,ipaddr,numsent,remains,recvlen,errno,strerror(errno),sock);
                    break;
                }
            } else if (remains > 0) {
                remains -= numsent;
                i += numsent;
                if (remains > 0)
                    printf("iguana sent.%d remains.%d of recvlen.%d (%s)\n", numsent, remains, recvlen, jsonbuf);
            }
        }
        if (retstr != space && retstr != space2) {
            //printf("free retstr.%p\n",retstr);
            free(retstr);
        }
    }
    memset(space, 0, sizeof(space));
    memset(space2, 0, sizeof(space2));
    //printf("free jsonbuf.%p\n",jsonbuf);
    free(jsonbuf);
    closesocket(sock);
    /*if ( 1 )
    {
        portable_mutex_lock(&LP_gcmutex);
        DL_APPEND(LP_garbage_collector,req);
        portable_mutex_unlock(&LP_gcmutex);
    }
    else*/
    {
        //printf("free req.%p\n",req);
        free(req);
    }
    if (spawned > 0)
        spawned--;
    return (0);
}

int32_t iguana_socket(int32_t bindflag, char* hostname, uint16_t port)
{
    int32_t opt, sock, result;
    char ipaddr[64], checkipaddr[64];
    struct timeval timeout;
    struct sockaddr_in saddr;
    socklen_t addrlen, slen;
    addrlen = sizeof(saddr);

    /**
     * gethostbyname() is deprecated and cause crash on x64 windows
     * the solution is to implement similar functionality by using getaddrinfo()
     * it is standard posix function and is correctly supported in win32/win64/linux
     * @author - fadedreamz@gmail.com
     */
    struct addrinfo* addrresult = NULL;
    struct addrinfo* returnptr = NULL;
    struct addrinfo hints;
    struct sockaddr_in* sockaddr_ipv4;
    int retVal;
    int found = 0;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (parse_ipaddr(ipaddr, hostname) != 0)
        port = parse_ipaddr(ipaddr, hostname);

    retVal = getaddrinfo(ipaddr, NULL, &hints, &addrresult);
    for (returnptr = addrresult; returnptr != NULL && found == 0; returnptr = returnptr->ai_next) {
        switch (returnptr->ai_family) {
        case AF_INET:
            sockaddr_ipv4 = (struct sockaddr_in*)returnptr->ai_addr;
            // we want to break from the loop after founding the first ipv4 address
            found = 1;
            break;
        }
    }

    // if we iterate through the loop and didn't find anything,
    // that means we failed in the dns lookup
    if (found == 0) {
        printf("getaddrinfo(%s) returned error\n", hostname);
        freeaddrinfo(addrresult);
        return (-1);
    }
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    //#ifdef _WIN32
    //   saddr.sin_addr.s_addr = (uint32_t)calc_ipbits("127.0.0.1");
    //#else

    saddr.sin_addr.s_addr = sockaddr_ipv4->sin_addr.s_addr;
    // graceful cleanup
    sockaddr_ipv4 = NULL;
    freeaddrinfo(addrresult);
    expand_ipbits(checkipaddr, saddr.sin_addr.s_addr);
    if (strcmp(ipaddr, checkipaddr) != 0)
        printf("bindflag.%d iguana_socket mismatch (%s) -> (%s)?\n", bindflag, checkipaddr, ipaddr);
    //#endif
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        if (errno != ETIMEDOUT)
            printf("socket() failed: %s errno.%d", strerror(errno), errno);
        return (-1);
    }
    opt = 1;
    slen = sizeof(opt);
    //printf("set keepalive.%d\n",setsockopt(sock,SOL_SOCKET,SO_KEEPALIVE,(void *)&opt,slen));
#ifndef _WIN32
    if (1) //&& bindflag != 0 )
    {
        opt = 0;
        getsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void*)&opt, &slen);
        opt = 1;
        //printf("keepalive.%d\n",opt);
    }
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));
#ifdef __APPLE__
    setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
#endif
#endif
    if (bindflag == 0) {
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (void*)&timeout, sizeof(timeout));
        result = connect(sock, (struct sockaddr*)&saddr, addrlen);
        if (result != 0) {
            if (errno != ECONNRESET && errno != ENOTCONN && errno != ECONNREFUSED && errno != ETIMEDOUT && errno != EHOSTUNREACH) {
                //printf("%s(%s) port.%d failed: %s sock.%d. errno.%d\n",bindflag!=0?"bind":"connect",hostname,port,strerror(errno),sock,errno);
            }
            if (sock >= 0)
                closesocket(sock);
            return (-1);
        }
        timeout.tv_sec = 10000000;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (void*)&timeout, sizeof(timeout));
    } else {
        while ((result = bind(sock, (struct sockaddr*)&saddr, addrlen)) != 0) {
            if (errno == EADDRINUSE) {
                sleep(1);
                printf("ERROR BINDING PORT.%d. this is normal tcp timeout, unless another process is using port\n", port);
                sleep(3);
                printf("%s(%s) port.%d try again: %s sock.%d. errno.%d\n", bindflag != 0 ? "bind" : "connect", hostname, port, strerror(errno), sock, errno);
                if (bindflag == 1) {
                    closesocket(sock);
                    return (-1);
                }
                sleep(13);
                //continue;
            }
            if (errno != ECONNRESET && errno != ENOTCONN && errno != ECONNREFUSED && errno != ETIMEDOUT && errno != EHOSTUNREACH) {
                printf("%s(%s) port.%d failed: %s sock.%d. errno.%d\n", bindflag != 0 ? "bind" : "connect", hostname, port, strerror(errno), sock, errno);
                closesocket(sock);
                return (-1);
            }
        }
        if (listen(sock, 512) != 0) {
            printf("listen(%s) port.%d failed: %s sock.%d. errno.%d\n", hostname, port, strerror(errno), sock, errno);
            if (sock >= 0)
                closesocket(sock);
            return (-1);
        }
    }
#ifdef __APPLE__
    //timeout.tv_sec = 0;
    //timeout.tv_usec = 30000;
    //setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(void *)&timeout,sizeof(timeout));
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (void*)&timeout, sizeof(timeout));
#endif
    return (sock);
}

void* NSPV_rpcloop(void* args)
{
    uint16_t port;
    int32_t retval, sock = -1, bindsock = -1;
    socklen_t clilen;
    struct sockaddr_in cli_addr;
    uint32_t ipbits, localhostbits;
    struct rpcrequest_info* req;
#ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(1, 1), &wsa_data);
#endif
    if ((port = *(uint16_t*)args) == 0)
        port = 7889;
    printf("Start NSPV_rpcloop.%u\n", port);
    localhostbits = (uint32_t)calc_ipbits(NSPV_externalip);
    //initial_bindsock_reset = LP_bindsock_reset;
    while (NSPV_STOP_RECEIVED == 0) //LP_bindsock_reset == initial_bindsock_reset )
    {
        //printf("LP_bindsock.%d\n",LP_bindsock);
        if (bindsock < 0) {
            if (strcmp(NSPV_externalip, "127.0.0.1") == 0) {
                while ((bindsock = iguana_socket(1, "0.0.0.0", port)) < 0)
                    usleep(10000);
            } else {
                while ((bindsock = iguana_socket(1, NSPV_externalip, port)) < 0)
                    usleep(10000);
            }
#ifndef _WIN32
            //fcntl(bindsock, F_SETFL, fcntl(bindsock, F_GETFL, 0) | O_NONBLOCK);
#endif
            //if ( counter++ < 1 )
            printf(">>>>>>>>>> NSPV_rpcloop %s:%d bind sock.%d API enabled at unixtime.%u <<<<<<<<<\n", NSPV_externalip, port, bindsock, (uint32_t)time(NULL));
        }
        //printf("after sock.%d\n",sock);
        clilen = sizeof(cli_addr);
        sock = accept(bindsock, (struct sockaddr*)&cli_addr, &clilen);
        if (sock < 0) {
            printf("NSPV_rpcloop ERROR on accept port.%u usock.%d errno %d %s\n", port, sock, errno, strerror(errno));
            closesocket(bindsock);
            bindsock = -1;
            continue;
        }
        memcpy(&ipbits, &cli_addr.sin_addr.s_addr, sizeof(ipbits));
        //printf("port.%u got incoming from %x\n",port,ipbits);
        if (DOCKERFLAG != 0 && (DOCKERFLAG == 1 || ipbits == DOCKERFLAG))
            ipbits = localhostbits;
        if (ipbits != localhostbits) // port == RPC_port &&
        {
            //printf("port.%u RPC_port.%u ipbits %x != %x\n",port,RPC_port,ipbits,localhostbits);
            closesocket(sock);
            continue;
        }
        req = calloc(1, sizeof(*req));
        //printf("LP_rpc_processreq req.%p\n",req);
        req->sock = sock;
        req->ipbits = ipbits;
        req->port = port;
        while (rpcdepth > 0) {
            fprintf(stderr, "wait for rpcdepth.%d to 0\n", rpcdepth);
            sleep(1);
        }
        rpcdepth++;
        LP_rpc_processreq(req);
        rpcdepth--;
        //OS_thread_create(&req->T,NULL,LP_rpc_processreq,req);
    }
    printf("i got killed\n");
    return (0);
}

#endif // NSPV_RPC_H
