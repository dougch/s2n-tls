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

#pragma once

#include "stuffer/s2n_stuffer.h"
#include "utils/s2n_result.h"

struct s2n_key_material;

/* Buffer len for receiving alerts during shutdown.
 *
 * s2n-tls will attempt to read until a close_notify is received. We allocate
 * a sufficiently large buffer in case the peer has sent some application
 * data that must be read first. */
#define KTLS_RECV_BUFFER_SIZE (1 << 12)

/* Define headers needed to enable and use kTLS.
 *
 * The inline header definitions are required to compile kTLS specific code.
 * kTLS has been tested on linux. For all other platforms, kTLS is marked as
 * unsupported, and will return an unsupported error.
 */
#include "tls/s2n_ktls_parameters.h"

/* A set of kTLS configurations representing the combination of sending
 * and receiving. */
typedef enum {
    /* Enable kTLS for the send socket. */
    S2N_KTLS_MODE_SEND,
    /* Enable kTLS for the receive socket. */
    S2N_KTLS_MODE_RECV,
} s2n_ktls_mode;

bool s2n_ktls_is_supported_on_platform();
S2N_RESULT s2n_ktls_retrieve_file_descriptor(struct s2n_connection *conn, s2n_ktls_mode ktls_mode, int *fd);

/* These functions will be part of the public API. */
int s2n_connection_ktls_enable_send(struct s2n_connection *conn);
int s2n_connection_ktls_enable_recv(struct s2n_connection *conn);
bool s2n_connection_is_ktls_enabled(struct s2n_connection *conn, s2n_ktls_mode ktls_mode);

/* ktls_io.c
 *
 * TODO: check that the following states are correctly set
 * D - error_alert_received
 * D - close_notify_received
 * D - read_closed
 * D - write_closed
 *
 * ?? whats the point of `writer_alert_out`
 *
 * TODO: notes on recv and send from 1on1s
 * - how to set conn->in_status PLAINTEXT vs ENCRYPTED
 * - handle error if cannot parse record type
 *
 * - does kTLS support peek?
 * - think about recv_multi_record
 * - test recv 1 alert fragment.. does ktls buffer until it has entire alert record?
 *   - reject fragmented alert?
 *
 * D - send5, recv10, send5, recv5.. does the first recv block?
 * D - handle recv 0.. peer shutdown
 * D - move s2n_ktls_recv in s2n_read_full_record
 * D - separate method s2n_ktls_recv_alert
 *   D - read alert into conn->in.. resize and all
 * D - test recv record: sending multi record. does kTLS read 1 or more records?
 * D - test recv/send partial app
 * D - no outstanding data when enabling ktls
 * D - test sending multiple iov
 * D - test shutdown
 * D - what happens if the size of the recv buffer is not large enough (fatal or retry error)?
 *   D - multiple reads to handle large amounts of data
 *
 */

/* SEND
 * D - alert:
 * - warnings:
 * - app:
 * (NOT SUPPORTED) - handshake:
 */
S2N_RESULT s2n_ktls_send(
        struct s2n_connection *conn, struct iovec *msg_iov, size_t count,
        uint8_t record_type, s2n_blocked_status *blocked, ssize_t *send_len);

/* RECV
 * app:
 * - s2n_recv(buf, size, blocked)
 *   - update buf and size based on size read
 *   - 0 if shutdown by peer
 * - document that ktls operates in multi record mode
 * - does blocked status propagate?
 * - what happens with conn->in_status
 * - what happens if provided buffer is smaller than record len
 *
 *
 * socket:
 * - s2n_ktls_recv
 *
 */
/* recv and get record_type */
S2N_RESULT s2n_ktls_recv(
        struct s2n_connection *conn, uint8_t *buf, ssize_t size,
        uint8_t *record_type, s2n_blocked_status *blocked, ssize_t *bytes_read);

/* testing */
struct s2n_ktls_io_ctx {
    struct s2n_stuffer ancillary;
    struct s2n_stuffer data;
    uint8_t send_record_type;
};
