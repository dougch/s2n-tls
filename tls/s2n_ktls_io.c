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
#include "error/s2n_errno.h"
#include "stuffer/s2n_stuffer.h"
#include "tls/s2n_ktls.h"
#include "utils/s2n_safety_macros.h"
#include "utils/s2n_socket.h"

S2N_RESULT s2n_ktls_send_control_msg(struct msghdr *msg, uint8_t record_type);

/* These variables are used to disable ktls mechanisms during testing. */
static bool ktls_mock_io_for_testing = false;

static ssize_t sendmsg_io(void *ctx, const struct msghdr *msg)
{
    int *sock = (int *) ctx;
    return sendmsg(*sock, msg, 0);
}

static ssize_t recvmsg_io(void *ctx, struct msghdr *msg)
{
    int *sock = (int *) ctx;
    return recvmsg(*sock, msg, 0);
}

static ssize_t send_stuffer_io(void *ctx, struct msghdr *msg)
{
    POSIX_ENSURE_REF(ctx);
    struct s2n_ktls_io_ctx *io_ctx = (struct s2n_ktls_io_ctx *) ctx;
    POSIX_ENSURE_REF(io_ctx);
    POSIX_ENSURE(io_ctx->ancillary.growable, S2N_ERR_SAFETY);
    POSIX_GUARD_RESULT(s2n_stuffer_validate(&io_ctx->data));
    POSIX_GUARD_RESULT(s2n_stuffer_validate(&io_ctx->ancillary));

    if (msg == NULL) {
        return 0;
    }

    size_t total_len = 0;

    for (size_t count = 0; count < msg->msg_iovlen; count++) {
        io_ctx->data = io_ctx->data;

        uint8_t *buf = msg->msg_iov[count].iov_base;
        size_t len = msg->msg_iov[count].iov_len;
        if (buf == NULL) {
            return 0;
        }

        if (s2n_stuffer_write_bytes(&io_ctx->data, buf, len) < 0) {
            errno = EAGAIN;
            return -1;
        }

        /* write record type and len if data was written successfully */
        if (s2n_stuffer_write_uint8(&io_ctx->ancillary, io_ctx->send_record_type) < 0) {
            errno = EAGAIN;
            return -1;
        }
        if (s2n_stuffer_write_uint8(&io_ctx->ancillary, len) < 0) {
            errno = EAGAIN;
            return -1;
        }

        total_len += len;
    }

    return total_len;
}

/* The following recv IO is based on observed kTLS behavior. */
static ssize_t recv_stuffer_io(void *ctx, struct msghdr *msg)
{
    POSIX_ENSURE_REF(ctx);
    struct s2n_ktls_io_ctx *io_ctx = (struct s2n_ktls_io_ctx *) ctx;
    POSIX_ENSURE_REF(io_ctx);
    POSIX_GUARD_RESULT(s2n_stuffer_validate(&io_ctx->data));
    POSIX_GUARD_RESULT(s2n_stuffer_validate(&io_ctx->ancillary));

    if (msg == NULL) {
        return 0;
    }

    /* we currently only recv on 1 iov */
    if (msg->msg_iovlen != 1) {
        errno = EINVAL;
        return -1;
    }

    uint8_t *buf = msg->msg_iov->iov_base;
    size_t total_requested_len = msg->msg_iov->iov_len;
    if (buf == NULL) {
        return 0;
    }

    int total_read = 0;
    int n_read = 0;
    uint8_t record_type = 0;
    char next_record_type = 0;
    uint8_t n_avail = 0;
    while (record_type == next_record_type) {
        if (s2n_stuffer_data_available(&io_ctx->ancillary) < 2) {
            errno = EAGAIN;
            return -1;
        }

        POSIX_GUARD(s2n_stuffer_read_uint8(&io_ctx->ancillary, &record_type));
        /* read the number of bytes requested or less if it isn't available */
        POSIX_GUARD(s2n_stuffer_read_uint8(&io_ctx->ancillary, &n_avail));
        /* sanity check that the send impl wrote enough data to stuffer */
        POSIX_ENSURE_LTE(n_avail, s2n_stuffer_data_available(&io_ctx->data));
        /* printf("\n------------------- here1 %d %d %d", record_type, next_record_type, s2n_stuffer_data_available(&io_ctx->ancillary)); */

        /* Compared to userspace TLS where we read all data available and then store it
         * in conn->in, kTLS will not return more than buf len. The extra data is
         * essentially stored in a kernel.
         *
         * Userspace impl: `n_read = (len < n_avail) ? len : n_avail;`
         */
        n_read = MIN(total_requested_len, n_avail);
        /* printf("\n------------------- here5 n_read:%d total_req:%zu n_avail:%d", n_read, total_requested_len, n_avail); */

        if (n_read == 0) {
            errno = EINVAL;
            return -1;
        }

        POSIX_GUARD(s2n_stuffer_read_bytes(&io_ctx->data, buf + total_read, n_read));

#if S2N_KTLS_SUPPORTED /* CMSG_* macros are platform specific */
        /* set ancillary data */
        struct cmsghdr *hdr = CMSG_FIRSTHDR(msg);
        hdr->cmsg_level = S2N_SOL_TLS;
        hdr->cmsg_type = S2N_TLS_GET_RECORD_TYPE; /* set to `GET_RECORD_TYPE` when receiving */
        hdr->cmsg_len = CMSG_LEN(sizeof(record_type));
        POSIX_CHECKED_MEMCPY(CMSG_DATA(hdr), &record_type, sizeof(record_type));
#endif

        /* since only part of the record was read, update the ancillary data with
         * the remaining length */
        if (n_read < n_avail) {
            /* printf("\n------------------- babaS"); */
            /* rewind and rewrite the length */
            POSIX_GUARD(s2n_stuffer_rewind_read(&io_ctx->ancillary, 1));
            uint8_t *ptr = s2n_stuffer_raw_read(&io_ctx->ancillary, 1);
            POSIX_CHECKED_MEMSET(ptr, n_avail - n_read, 1);
            /* rewind to re-read the record with the remaining length */
            POSIX_GUARD(s2n_stuffer_rewind_read(&io_ctx->ancillary, 2));
        }
        total_requested_len -= n_read;
        total_read += n_read;

        /* if we already read the requested amount then return */
        if (total_requested_len == 0) {
            break;
        } else if (total_requested_len) {
            /* otherwise attempt to read the next record type if available */
            int peek_ret = s2n_stuffer_peek_char(&io_ctx->ancillary, &next_record_type);
            /* printf("\n------------------- H&US %d", peek_ret); */
            /* no more records */
            if (peek_ret < 0) {
                break;
            }
        }

        /* printf("\n------------------- here2 n_read:%d total_req:%zu nxt:%d total_read:%d ", n_read, total_requested_len, next_record_type, total_read); */
    }

    /* printf("\n------------------- here222 n_read:%d total_req:%zu nxt:%d total_read:%d ", n_read, total_requested_len, next_record_type, total_read); */

    return total_read;
}

/*
 * sendmsg and recvmsg are syscalls which can be used to send 'real' data along
 * with ancillary data. Ancillary data is used to communicate to the socket the
 * type of the TLS record being sent/received.
 *
 * Ancillary data macros (CMSG_*) are platform specific and gated.
 */

S2N_RESULT s2n_ktls_send_msg_impl(struct s2n_connection *conn, int sock, struct msghdr *msg,
        struct iovec *msg_iov, size_t count, s2n_blocked_status *blocked,
        ssize_t *send_len)
{
    RESULT_ENSURE_REF(msg);
    RESULT_ENSURE_REF(msg_iov);
    RESULT_ENSURE_REF(blocked);
    RESULT_ENSURE_REF(send_len);
    RESULT_ENSURE_GT(count, 0);

    /* set send buffer */
    msg->msg_iov = msg_iov;
    msg->msg_iovlen = count;

    *blocked = S2N_BLOCKED_ON_WRITE;

    if (ktls_mock_io_for_testing) {
        RESULT_ENSURE(s2n_in_unit_test(), S2N_ERR_NOT_IN_UNIT_TEST);
        RESULT_ENSURE_REF(conn->send_io_context);
        *send_len = send_stuffer_io(conn->send_io_context, msg);
    } else {
        *send_len = sendmsg_io(&sock, msg);
    }

    if (*send_len < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            RESULT_BAIL(S2N_ERR_IO_BLOCKED);
        }
    }

    *blocked = S2N_NOT_BLOCKED;

    return S2N_RESULT_OK;
}

S2N_RESULT s2n_ktls_send_control_msg(struct msghdr *msg, uint8_t record_type)
{
    RESULT_ENSURE_REF(msg);

#if S2N_KTLS_SUPPORTED /* CMSG_* macros are platform specific */
    /* set ancillary data */
    struct cmsghdr *hdr = CMSG_FIRSTHDR(msg);
    hdr->cmsg_level = S2N_SOL_TLS;
    hdr->cmsg_type = S2N_TLS_SET_RECORD_TYPE;
    hdr->cmsg_len = CMSG_LEN(sizeof(record_type));
    RESULT_CHECKED_MEMCPY(CMSG_DATA(hdr), &record_type, sizeof(record_type));
#endif

    return S2N_RESULT_OK;
}

/* Best practices taken from
 * https://man7.org/tlpi/code/online/dist/sockets/scm_cred_send.c.html */
S2N_RESULT s2n_ktls_send_msg(
        struct s2n_connection *conn, int sock, uint8_t record_type, struct iovec *msg_iov,
        size_t count, s2n_blocked_status *blocked, ssize_t *send_len)
{
    RESULT_ENSURE_REF(msg_iov);
    RESULT_ENSURE_REF(msg_iov->iov_base);
    RESULT_ENSURE_GT(msg_iov->iov_len, 0);
    RESULT_ENSURE_GT(count, 0);
    RESULT_ENSURE_REF(blocked);
    RESULT_ENSURE_REF(send_len);

    /* Init msghdr
     *
     * The control message buffer must be zero-initialized in order for the
     * CMSG_NXTHDR macro to work correctly. However we skip this step since
     * CMSG_NXTHDR is not used for the kTLS send implementation and this code
     * path is performance sensitive.
     */
    struct msghdr msg = { 0 };
    msg.msg_name = NULL;
    msg.msg_namelen = 0;

#if S2N_KTLS_SUPPORTED /* CMSG_* macros are platform specific */
    /* Allocate a char array of suitable size to hold the ancillary data.
     * However, since this buffer is in reality a 'struct cmsghdr', use a
     * union to ensure that it is aligned as required for that structure.
     */
    union {
        char buf[CMSG_SPACE(sizeof(record_type))];
        /* Space large enough to hold a record_type structure */
        struct cmsghdr align;
    } control_msg;

    msg.msg_control = control_msg.buf;
    msg.msg_controllen = sizeof(control_msg.buf);
#endif

    RESULT_GUARD(s2n_ktls_send_control_msg(&msg, record_type));
    RESULT_GUARD(s2n_ktls_send_msg_impl(conn, sock, &msg, msg_iov, count, blocked, send_len));

    return S2N_RESULT_OK;
}

S2N_RESULT s2n_ktls_recv_msg_impl(struct s2n_connection *conn, int sock, struct msghdr *msg,
        struct iovec *msg_iov, s2n_blocked_status *blocked, ssize_t *bytes_read)
{
    RESULT_ENSURE_REF(msg);
    RESULT_ENSURE_REF(msg_iov);
    RESULT_ENSURE_REF(blocked);
    RESULT_ENSURE_REF(bytes_read);

    /* set receive buffer */
    msg->msg_iov = msg_iov;
    msg->msg_iovlen = 1;

    *blocked = S2N_BLOCKED_ON_READ;
    if (ktls_mock_io_for_testing) {
        RESULT_ENSURE(s2n_in_unit_test(), S2N_ERR_NOT_IN_UNIT_TEST);
        RESULT_ENSURE_REF(conn->recv_io_context);
        *bytes_read = recv_stuffer_io(conn->recv_io_context, msg);
    } else {
        *bytes_read = recvmsg_io(&sock, msg);
    }

    if (*bytes_read == 0) {
        *bytes_read = 0;
        s2n_atomic_flag_set(&conn->read_closed);
        RESULT_BAIL(S2N_ERR_CLOSED);
    } else if (*bytes_read < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            RESULT_BAIL(S2N_ERR_IO_BLOCKED);
        }
        RESULT_BAIL(S2N_ERR_IO);
    }
    *blocked = S2N_NOT_BLOCKED;

    return S2N_RESULT_OK;
}

S2N_RESULT s2n_ktls_recv_control_msg(struct msghdr *msg, uint8_t *record_type)
{
    RESULT_ENSURE_REF(msg);
    RESULT_ENSURE_REF(record_type);

#if S2N_KTLS_SUPPORTED /* CMSG_* macros are platform specific */
    /* attempt to read the first ancillary data */
    struct cmsghdr *hdr = CMSG_FIRSTHDR(msg);
    RESULT_ENSURE(hdr != NULL, S2N_ERR_IO);

    /* iterate over all headers until it matches RECORD_TYPE. CMSG_NXTHDR will
     * return NULL if there are no more cmsg */
    while (hdr != NULL) {
        if (hdr->cmsg_level == S2N_SOL_TLS && hdr->cmsg_type == S2N_TLS_GET_RECORD_TYPE) {
            *record_type = *(unsigned char *) CMSG_DATA(hdr);
            break;
        }

        /* attempt to get the next ancillary data */
        hdr = CMSG_NXTHDR(msg, hdr);
    }
#endif

    if (*record_type == 0) {
        /* FIXME return proper error */
        RESULT_BAIL(S2N_ERR_SAFETY);
    }

    return S2N_RESULT_OK;
}

/* Best practices taken from
 * https://man7.org/tlpi/code/online/dist/sockets/scm_cred_recv.c.html */
S2N_RESULT s2n_ktls_recv_msg(struct s2n_connection *conn, int sock, uint8_t *buf, size_t length,
        uint8_t *record_type, s2n_blocked_status *blocked, ssize_t *bytes_read)
{
    RESULT_ENSURE_REF(buf);
    RESULT_ENSURE_REF(record_type);
    RESULT_ENSURE_REF(blocked);
    RESULT_ENSURE_REF(bytes_read);
    RESULT_ENSURE_GT(length, 0);

    /* Init msghdr */
    struct msghdr msg = { 0 };
    msg.msg_name = NULL;
    msg.msg_namelen = 0;

#if S2N_KTLS_SUPPORTED /* CMSG_* macros are platform specific */
    /* Allocate a char array of suitable size to hold the ancillary data.
     * However, since this buffer is in reality a 'struct cmsghdr', use a
     * union to ensure that it is aligned as required for that structure.
     */
    union {
        char buf[CMSG_SPACE(sizeof(record_type))];
        /* Space large enough to hold a record_type structure */
        struct cmsghdr align;
    } control_msg;

    msg.msg_control = control_msg.buf;
    msg.msg_controllen = sizeof(control_msg.buf);
#endif

    struct iovec msg_iov;
    msg_iov.iov_base = buf;
    msg_iov.iov_len = length;

    /* receive msg */
    RESULT_GUARD(s2n_ktls_recv_msg_impl(conn, sock, &msg, &msg_iov, blocked, bytes_read));

    RESULT_GUARD(s2n_ktls_recv_control_msg(&msg, record_type));

    return S2N_RESULT_OK;
}

S2N_RESULT s2n_ktls_send(
        struct s2n_connection *conn, struct iovec *msg_iov, size_t count,
        uint8_t record_type, s2n_blocked_status *blocked, ssize_t *send_len)
{
    RESULT_ENSURE_REF(conn);
    RESULT_ENSURE_REF(msg_iov);
    RESULT_ENSURE_REF(msg_iov->iov_base);
    RESULT_ENSURE_GT(msg_iov->iov_len, 0);
    RESULT_ENSURE_GT(count, 0);
    RESULT_ENSURE_REF(blocked);
    RESULT_ENSURE_REF(send_len);

    int fd = 0;
    if (!ktls_mock_io_for_testing) {
        RESULT_GUARD(s2n_ktls_retrieve_file_descriptor(conn, S2N_KTLS_MODE_SEND, &fd));
    }

    if (record_type == TLS_HANDSHAKE) {
        /* handshake messages not supported for kTLS 1.2 */
        RESULT_BAIL(S2N_ERR_UNIMPLEMENTED);
    }
    RESULT_GUARD(s2n_ktls_send_msg(conn, fd, record_type, msg_iov, count, blocked, send_len));

    return S2N_RESULT_OK;
}

S2N_RESULT s2n_ktls_recv(
        struct s2n_connection *conn, uint8_t *buf, ssize_t size,
        uint8_t *record_type, s2n_blocked_status *blocked, ssize_t *bytes_read)
{
    int fd = 0;
    if (!ktls_mock_io_for_testing) {
        RESULT_GUARD(s2n_ktls_retrieve_file_descriptor(conn, S2N_KTLS_MODE_SEND, &fd));
    }

    RESULT_GUARD(s2n_ktls_recv_msg(conn, fd, buf, size, record_type, blocked, bytes_read));

    if (*record_type == TLS_ALERT) {
        RESULT_GUARD(s2n_stuffer_validate(&conn->in));
        RESULT_GUARD_POSIX(s2n_stuffer_resize_if_empty(&conn->in, S2N_ALERT_LENGTH));

        /* reset conn->in and write alert so it can be processed later */
        RESULT_GUARD_POSIX(s2n_stuffer_rewrite(&conn->in));
        RESULT_GUARD_POSIX(s2n_stuffer_write_bytes(&conn->in, buf, S2N_ALERT_LENGTH));
    } else if (*record_type == TLS_HANDSHAKE) {
        RESULT_ENSURE_GTE(*bytes_read, 1);
        uint8_t handshake_type = buf[0];
        if (handshake_type == TLS_HELLO_REQUEST) {
            /* renegotiation is not supported for kTLS. Ignore the message. */
            RESULT_ENSURE(!conn->config->renegotiate_request_cb, S2N_ERR_NO_RENEGOTIATION);
            *bytes_read = 0;
            return S2N_RESULT_OK;
        }
        RESULT_BAIL(S2N_ERR_BAD_MESSAGE);
    }

    return S2N_RESULT_OK;
}

/* Use for testing only. */
S2N_RESULT s2n_ktls_mock_io_for_testing(void)
{
    RESULT_ENSURE(s2n_in_unit_test(), S2N_ERR_NOT_IN_UNIT_TEST);

    ktls_mock_io_for_testing = true;

    return S2N_RESULT_OK;
}
