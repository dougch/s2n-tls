/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <sys/socket.h>

#include "bits/stdint-uintn.h"
#include "stuffer/s2n_stuffer.h"
#include "tls/s2n_ktls.h"
#include "utils/s2n_safety_macros.h"
#include "utils/s2n_socket.h"

/*
 * sendmsg and recvmsg are syscalls which can be used to send 'real' data along
 * with ancillary data. Ancillary data is used to communicate to the socket the
 * type of the TLS record being sent/received.
 *
 * Ancillary data macros (CMSG_*) are platform specific and gated.
 */

S2N_RESULT s2n_ktls_send_msg_impl(int sock, struct msghdr *msg,
        struct iovec *msg_iov, size_t count, s2n_blocked_status *blocked,
        ssize_t *result)
{
    RESULT_ENSURE_REF(msg);
    RESULT_ENSURE_REF(msg_iov);
    RESULT_ENSURE_REF(blocked);
    RESULT_ENSURE_REF(result);
    RESULT_ENSURE_GT(count, 0);

    /* printf("\n----------- s2n_ktls_send_msg_impl %zu \n", msg_iov->iov_len); */

    /* FIXME handle multple counts? add test */
    /* set send buffer */
    msg->msg_iov = msg_iov;
    msg->msg_iovlen = count;

    *blocked = S2N_BLOCKED_ON_WRITE;

    *result = sendmsg(sock, msg, 0);
    if (*result < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            RESULT_BAIL(S2N_ERR_IO_BLOCKED);
        }
        RESULT_BAIL(S2N_ERR_IO);
    }

    *blocked = S2N_NOT_BLOCKED;

    return S2N_RESULT_OK;
}

S2N_RESULT s2n_ktls_send_control_msg(int sock, struct msghdr *msg,
        uint8_t record_type, s2n_blocked_status *blocked, ssize_t *result)
{
    RESULT_ENSURE_REF(msg);
    RESULT_ENSURE_REF(blocked);
    RESULT_ENSURE_REF(result);

#if S2N_KTLS_SUPPORTED
    RESULT_ENSURE_REF(msg->msg_control);
    RESULT_ENSURE_GTE(msg->msg_controllen, CMSG_SPACE(sizeof(record_type)));

    /* set ancillary data */
    struct cmsghdr *hdr = CMSG_FIRSTHDR(msg);
    hdr->cmsg_level = S2N_SOL_TLS;
    hdr->cmsg_type = S2N_TLS_SET_RECORD_TYPE;
    hdr->cmsg_len = CMSG_LEN(sizeof(record_type));
    RESULT_CHECKED_MEMCPY(CMSG_DATA(hdr), &record_type, sizeof(record_type));
#else
    RESULT_BAIL(S2N_ERR_KTLS_UNSUPPORTED_PLATFORM);
#endif

    return S2N_RESULT_OK;
}

/* Best practices taken from
 * https://man7.org/tlpi/code/online/dist/sockets/scm_cred_send.c.html */
S2N_RESULT s2n_ktls_send_msg(
        int sock, uint8_t record_type, struct iovec *msg_iov,
        size_t count, s2n_blocked_status *blocked, ssize_t *result)
{
    RESULT_ENSURE_REF(msg_iov);
    RESULT_ENSURE_REF(msg_iov->iov_base);
    RESULT_ENSURE_GT(msg_iov->iov_len, 0);
    RESULT_ENSURE_GT(count, 0);
    RESULT_ENSURE_REF(blocked);
    RESULT_ENSURE_REF(result);

    /* Init msghdr
     *
     * The control message buffer must be zero-initialized in order for the
     * CMSG_NXTHDR() macro to work correctly. However since we dont use
     * CMSG_NXTHDR for kTLS and this code path is performance sensitive we skip
     * this step.
     */
    struct msghdr msg = { 0 };

#if S2N_KTLS_SUPPORTED
    /* Allocate a char array of suitable size to hold the ancillary data.
     * However, since this buffer is in reality a 'struct cmsghdr', use a
     * union to ensure that it is aligned as required for that structure.
     */
    union {
        /* Space large enough to hold record_type */
        char buf[CMSG_SPACE(sizeof(record_type))];
        struct cmsghdr align;
    } control_msg;

    msg.msg_control = control_msg.buf;
    msg.msg_controllen = sizeof(control_msg.buf);
#endif

    RESULT_GUARD(s2n_ktls_send_control_msg(sock, &msg, record_type, blocked, result));

    RESULT_GUARD(s2n_ktls_send_msg_impl(sock, &msg, msg_iov, count, blocked, result));

    return S2N_RESULT_OK;
}

S2N_RESULT s2n_ktls_recv_msg_impl(struct s2n_connection *conn, int sock, struct msghdr *msg,
        struct iovec *msg_iov, s2n_blocked_status *blocked, ssize_t *result)
{
    RESULT_ENSURE_REF(msg);
    RESULT_ENSURE_REF(msg_iov);
    RESULT_ENSURE_REF(blocked);
    RESULT_ENSURE_REF(result);

    /* set receive buffer */
    msg->msg_iov = msg_iov;
    msg->msg_iovlen = 1;

    *blocked = S2N_BLOCKED_ON_READ;
    *result = recvmsg(sock, msg, 0);

    if (*result == 0) {
        s2n_atomic_flag_set(&conn->read_closed);
        RESULT_BAIL(S2N_ERR_CLOSED);
    } else if (*result < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            RESULT_BAIL(S2N_ERR_IO_BLOCKED);
        }
        RESULT_BAIL(S2N_ERR_IO);
    }
    *blocked = S2N_NOT_BLOCKED;

    return S2N_RESULT_OK;
}

S2N_RESULT s2n_ktls_recv_control_msg(int sock, struct msghdr *msg,
        uint8_t *record_type, s2n_blocked_status *blocked, ssize_t *result)
{
    RESULT_ENSURE_REF(msg);
    RESULT_ENSURE_REF(record_type);
    RESULT_ENSURE_REF(blocked);
    RESULT_ENSURE_REF(result);

#if S2N_KTLS_SUPPORTED
    RESULT_ENSURE_REF(msg->msg_control);
    RESULT_ENSURE_GTE(msg->msg_controllen, CMSG_SPACE(sizeof(record_type)));

    /* attempt to read the ancillary data */
    struct cmsghdr *hdr = CMSG_FIRSTHDR(msg);
    RESULT_ENSURE(hdr != NULL, S2N_ERR_IO);

    /* iterate over all headers until it matches RECORD_TYPE.
     * CMSG_NXTHDR will return NULL if there are no more cmsg */
    uint8_t temp_record_type = 0;
    while (hdr != NULL) {
        /* printf("------========-------=========-- recv control_msg %d %d\n", hdr->cmsg_level, hdr->cmsg_type); */

        if (hdr->cmsg_level == S2N_SOL_TLS && hdr->cmsg_type == S2N_TLS_GET_RECORD_TYPE) {
            temp_record_type = *(unsigned char *) CMSG_DATA(hdr);
            /* printf("-------------------------- recv S2N_TLS_GET_RECORD_TYPE %d\n", temp_record_type); */
            /* break; */
        }

        /* attempt to get the next header */
        hdr = CMSG_NXTHDR(msg, hdr);
    }

    /* confirm that its possible to get the header from the cmsg */
    RESULT_ENSURE(temp_record_type > 0, S2N_ERR_IO);
    *record_type = temp_record_type;

#else
    RESULT_BAIL(S2N_ERR_KTLS_UNSUPPORTED_PLATFORM);
#endif

    return S2N_RESULT_OK;
}

/* Best practices taken from
 * https://man7.org/tlpi/code/online/dist/sockets/scm_cred_recv.c.html */
S2N_RESULT s2n_ktls_recv_msg(struct s2n_connection *conn, int sock, uint8_t *buf, size_t length,
        uint8_t *record_type, s2n_blocked_status *blocked,
        ssize_t *result)
{
    RESULT_ENSURE_REF(buf);
    RESULT_ENSURE_REF(record_type);
    RESULT_ENSURE_REF(blocked);
    RESULT_ENSURE_REF(result);
    RESULT_ENSURE_GT(length, 0);

    /* Init msghdr */
    struct msghdr msg = { 0 };

#if S2N_KTLS_SUPPORTED
    /* Allocate a char array of suitable size to hold the ancillary data.
     * However, since this buffer is in reality a 'struct cmsghdr', use a
     * union to ensure that it is aligned as required for that structure.
     */
    union {
        /* create buf with addition space incase the application recieves
         * more than one cmsg */
        char buf[CMSG_SPACE(sizeof(uint32_t)) * 4];
        struct cmsghdr align;
    } control_msg;

    /* The control message buffer must be zero-initialized in order for the
     * CMSG_NXTHDR() macro to work correctly.
     *
     * https://man7.org/tlpi/code/online/dist/sockets/scm_cred_send.c.html
     */
    RESULT_CHECKED_MEMSET(&control_msg.buf, 0, sizeof(control_msg.buf));

    msg.msg_control = control_msg.buf;
    msg.msg_controllen = sizeof(control_msg.buf);
#endif

    struct iovec msg_iov;
    msg_iov.iov_base = buf;
    msg_iov.iov_len = length;

    /* receive msg */
    RESULT_GUARD(s2n_ktls_recv_msg_impl(conn, sock, &msg, &msg_iov, blocked, result));

    RESULT_GUARD(s2n_ktls_recv_control_msg(sock, &msg, record_type, blocked, result));

    return S2N_RESULT_OK;
}

S2N_RESULT s2n_ktls_send(
        struct s2n_connection *conn, struct iovec *msg_iov, size_t count,
        uint8_t record_type, s2n_blocked_status *blocked, ssize_t *result)
{
    RESULT_ENSURE_REF(conn);
    RESULT_ENSURE_REF(msg_iov);
    RESULT_ENSURE_REF(msg_iov->iov_base);
    RESULT_ENSURE_GT(msg_iov->iov_len, 0);
    RESULT_ENSURE_GT(count, 0);
    RESULT_ENSURE_REF(blocked);
    RESULT_ENSURE_REF(result);

    int fd;
    RESULT_GUARD(s2n_ktls_retrieve_file_descriptor(conn, S2N_KTLS_MODE_SEND, &fd));

    if (record_type == TLS_ALERT) {
        /* TODO dont need this case.. can remove. possibly have a separate fn for alert */
    } else if (record_type == TLS_HANDSHAKE) {
        /* handshake messages not supported for kTLS 1.2 */
        RESULT_BAIL(S2N_ERR_UNIMPLEMENTED);
    }
    RESULT_GUARD(s2n_ktls_send_msg(fd, record_type, msg_iov, count, blocked, result));

    return S2N_RESULT_OK;
}

S2N_RESULT s2n_ktls_recv(
        struct s2n_connection *conn, uint8_t *buf, ssize_t size,
        uint8_t *record_type, s2n_blocked_status *blocked, ssize_t *result)
{
    int fd = 0;
    RESULT_GUARD(s2n_ktls_retrieve_file_descriptor(conn, S2N_KTLS_MODE_RECV, &fd));

    RESULT_GUARD(s2n_ktls_recv_msg(conn, fd, buf, size, record_type, blocked, result));

    if (*record_type == TLS_ALERT) {
        /* printf("\n s2n_ktls_recv ---------- TLS_ALERT buf: %d %d result: %zd\n", buf[0], buf[1], *result); */

        /* copy alert to conn->in for processing later */
        RESULT_GUARD_POSIX(s2n_stuffer_write_bytes(&conn->in, buf, S2N_ALERT_LENGTH));
    } else if (*record_type == TLS_HANDSHAKE) {
        /* handshake messages not supported for kTLS 1.2. ignore the read */
        *result = 0;
    }

    return S2N_RESULT_OK;
}

S2N_RESULT s2n_ktls_recv_alert(struct s2n_connection *conn, uint8_t *record_type, s2n_blocked_status *blocked)
{
    RESULT_ENSURE_REF(conn);
    RESULT_GUARD(s2n_stuffer_validate(&conn->in));

    int fd = 0;
    RESULT_GUARD(s2n_ktls_retrieve_file_descriptor(conn, S2N_KTLS_MODE_RECV, &fd));

    /* TODO read into conn->in ? */
    /* /1* kTLS alert recv size 4096 *1/ */
    /* #define S2N_KTLS_RECV_ALERT_FRAGMENT_LENGTH (1 << 12) */
    /* uint32_t available_space = s2n_stuffer_space_remaining(&conn->in); */
    /* printf("\n===================== %d", available_space); */
    /* if (available_space < S2N_KTLS_RECV_ALERT_FRAGMENT_LENGTH) { */
    /*     RESULT_GUARD_POSIX(s2n_stuffer_growable_alloc(&conn->in, S2N_KTLS_RECV_ALERT_FRAGMENT_LENGTH)); */
    /* } */

    uint8_t alert[2] = { 0 };

    ssize_t _result = -1;

    RESULT_GUARD(s2n_ktls_recv_msg(conn, fd, alert, S2N_ALERT_LENGTH, record_type, blocked, &_result));

    if (*record_type == TLS_ALERT) {
        /* printf("\n s2n_ktls_recv_alert ---------- TLS_ALERT buf: %d %d result: %zd\n", alert[0], alert[1], _result); */

        /* copy alert to conn->in for processing later */
        RESULT_GUARD_POSIX(s2n_stuffer_write_bytes(&conn->in, alert, S2N_ALERT_LENGTH));
    }

    return S2N_RESULT_OK;
}
