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

#include "utils/s2n_result.h"

struct s2n_key_material;

/* Define headers needed to enable and use kTLS.
 *
 * The inline header definitions are required to compile kTLS specific code.
 * kTLS has been tested on linux. For all other platforms, kTLS is marked as
 * unsuppored, and will return an un-supported error.
 */
#if defined(__linux__)
    #define S2N_KTLS_SUPPORTED true
    #include "tls/s2n_ktls_linux.h"
#else
    #define S2N_KTLS_SUPPORTED false
    #include "tls/s2n_ktls_unsupported.h"
#endif

/* A set of kTLS configurations representing the combination of sending
 * and receiving.
 */
typedef enum {
    /* Enable kTLS for the send socket. */
    S2N_KTLS_MODE_SEND,
    /* Enable kTLS for the receive socket. */
    S2N_KTLS_MODE_RECV,
} s2n_ktls_mode;

bool s2n_ktls_is_supported_on_platform();
int s2n_connection_ktls_enable_send(struct s2n_connection *conn);
int s2n_connection_ktls_enable_recv(struct s2n_connection *conn);
bool s2n_connection_is_ktls_enabled(struct s2n_connection *conn, s2n_ktls_mode ktls_mode);
S2N_RESULT s2n_ktls_retrieve_file_descriptor(struct s2n_connection *conn, s2n_ktls_mode ktls_mode, int *fd);

/* ktls_io.c
 *
 * TODO: check that the following states are correctly set
 * - error_alert_received
 * - close_notify_received
 * - read_closed
 * - write_closed
 *
 * TODO: notes on recv and send from 1on1s
 * D - test recv record: sending multi record. does kTLS read 1 or more records?
 *   - think about recv_multi_record
 * - test send record with 1 alert byte.. do we recv 1 fragment or does ktls buffer until it has entire alert record
 *   - reject fragmented alert??
 *
 * - test sending multiple iov
 * - test recv/send partial alert/app
 * - read alert into conn->in.. resize and all
 *
 * - no outstanding data when enabling ktls
 * - dont need a separate method s2n_ktls_recv_alert
 * - does kTLS support peek?
 *
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
        struct s2n_connection *conn, struct iovec *bufs, size_t count,
        uint8_t record_type, s2n_blocked_status *blocked, ssize_t *result);

/* RECV
 * D - alert: s2n_read_full_record < s2n_shutdown
 * D - app: s2n_read_full_record < s2n_recv_impl
 * D - handshake: s2n_read_full_record < s2n_handshake_read_io
 */
/* recv and get record_type */
S2N_RESULT s2n_ktls_recv(
        struct s2n_connection *conn, uint8_t *buf, ssize_t size,
        uint8_t *record_type, s2n_blocked_status *blocked, ssize_t *result);

/* recv ALERT */
S2N_RESULT s2n_ktls_recv_alert(struct s2n_connection *conn, uint8_t *record_type, s2n_blocked_status *blocked);
