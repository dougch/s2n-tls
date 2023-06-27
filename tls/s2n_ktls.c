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

#include "tls/s2n_ktls.h"

#include "s2n.h"

#if defined(__linux__)
    #include <sys/socket.h>
#endif

#include "utils/s2n_socket.h"

/* These variables are used to disable ktls mechanisms during testing. */
static bool disable_ktls_socket_config_for_testing = false;

bool s2n_ktls_is_supported_on_platform()
{
#if S2N_KTLS_SUPPORTED
    return true;
#else
    return false;
#endif
}

S2N_RESULT s2n_ktls_validate(struct s2n_connection *conn, s2n_ktls_mode ktls_mode)
{
    RESULT_ENSURE_REF(conn);
    RESULT_ENSURE_REF(conn->secure);
    RESULT_ENSURE_REF(conn->secure->cipher_suite);
    RESULT_ENSURE_REF(conn->secure->cipher_suite->record_alg);
    const struct s2n_cipher *cipher = conn->secure->cipher_suite->record_alg->cipher;
    RESULT_ENSURE_REF(cipher);

    /* kTLS enable should only be called once the handshake has completed. */
    if (!is_handshake_complete(conn)) {
        RESULT_BAIL(S2N_ERR_KTLS_HANDSHAKE_NOT_COMPLETE);
    }

    /* TODO support TLS 1.3
     *
     * TLS 1.3 support requires sending the KeyUpdate message when the cryptographic
     * KeyLimits are met. However, this is currently only possible by applying a
     * kernel patch to support this functionality.
     */
    RESULT_ENSURE(conn->actual_protocol_version == S2N_TLS12, S2N_ERR_KTLS_UNSUPPORTED_CONN);

    /* Check if the cipher supports kTLS */
    RESULT_ENSURE(cipher->ktls_supported, S2N_ERR_KTLS_UNSUPPORTED_CONN);

    /* kTLS I/O functionality is managed by s2n-tls. kTLS cannot be enabled if the
     * application sets custom I/O (managed_send_io == false means application has
     * set custom I/O).
     */
    switch (ktls_mode) {
        case S2N_KTLS_MODE_SEND:
            RESULT_ENSURE_REF(conn->send_io_context);
            RESULT_ENSURE(conn->managed_send_io, S2N_ERR_KTLS_MANAGED_IO);
            break;
        case S2N_KTLS_MODE_RECV:
            RESULT_ENSURE_REF(conn->recv_io_context);
            RESULT_ENSURE(conn->managed_recv_io, S2N_ERR_KTLS_MANAGED_IO);
            break;
    }

    return S2N_RESULT_OK;
}

S2N_RESULT s2n_ktls_retrieve_file_descriptor(struct s2n_connection *conn, s2n_ktls_mode ktls_mode, int *fd)
{
    RESULT_ENSURE_REF(conn);
    RESULT_ENSURE_REF(fd);

    if (ktls_mode == S2N_KTLS_MODE_RECV) {
        /* retrieve the receive fd */
        RESULT_ENSURE_REF(conn->recv_io_context);
        const struct s2n_socket_read_io_context *peer_socket_ctx = conn->recv_io_context;
        *fd = peer_socket_ctx->fd;
    } else if (ktls_mode == S2N_KTLS_MODE_SEND) {
        /* retrieve the send fd */
        RESULT_ENSURE_REF(conn->send_io_context);
        const struct s2n_socket_write_io_context *peer_socket_ctx = conn->send_io_context;
        *fd = peer_socket_ctx->fd;
    }

    return S2N_RESULT_OK;
}

S2N_RESULT s2n_ktls_init_aes128_gcm_crypto_info(struct s2n_connection *conn, s2n_ktls_mode ktls_mode,
        struct s2n_key_material *key_material, struct s2n_tls12_crypto_info_aes_gcm_128 *crypto_info, int *tls_tx_rx_mode)
{
    RESULT_ENSURE_REF(conn);
    RESULT_ENSURE_REF(conn->client);
    RESULT_ENSURE_REF(conn->server);
    RESULT_ENSURE_REF(key_material);
    RESULT_ENSURE_REF(crypto_info);
    RESULT_ENSURE_REF(tls_tx_rx_mode);

    /* TODO once other ciphers and protocols are supported, check that the
     * negotiated cipher is AES_128_GCM
     *
     * This would involve adding a unique identifier to s2n_cipher.
     */

    /* set tls mode */
    if (ktls_mode == S2N_KTLS_MODE_SEND) { /* TX */
        *tls_tx_rx_mode = S2N_TLS_TX;
    } else { /* RX */
        *tls_tx_rx_mode = S2N_TLS_RX;
    }

    /* set values based on mode of operation */
    uint8_t *implicit_iv, *sequence_number = NULL;
    struct s2n_blob *key = NULL;

    if ((conn->mode == S2N_SERVER && ktls_mode == S2N_KTLS_MODE_SEND)        /* server sending */
            || (conn->mode == S2N_CLIENT && ktls_mode == S2N_KTLS_MODE_RECV) /* client receiving */
    ) {
        /* if server is sending or client is receiving then use server key material */
        key = &key_material->server_key;
        implicit_iv = conn->server->server_implicit_iv;
        sequence_number = conn->server->server_sequence_number;
    } else {
        key = &key_material->client_key;
        implicit_iv = conn->client->client_implicit_iv;
        sequence_number = conn->client->client_sequence_number;
    }

    RESULT_ENSURE_REF(key->data);
    RESULT_ENSURE_EQ(key->size, S2N_TLS_CIPHER_AES_GCM_128_KEY_SIZE);

    /* set values on crypto_info */
    crypto_info->info.cipher_type = S2N_TLS_CIPHER_AES_GCM_128;
    crypto_info->info.version = S2N_TLS_1_2_VERSION;
    RESULT_CHECKED_MEMCPY(crypto_info->salt, implicit_iv, S2N_TLS_CIPHER_AES_GCM_128_SALT_SIZE);
    RESULT_CHECKED_MEMCPY(crypto_info->rec_seq, sequence_number, S2N_TLS_CIPHER_AES_GCM_128_REC_SEQ_SIZE);
    RESULT_CHECKED_MEMCPY(crypto_info->key, key->data, S2N_TLS_CIPHER_AES_GCM_128_KEY_SIZE);
    RESULT_CHECKED_MEMCPY(crypto_info->iv, implicit_iv, S2N_TLS_CIPHER_AES_GCM_128_IV_SIZE);

    /* TODO used for debugging remove before PR */
    if (false) {
        for (int i = 0; i < 4; i++) {
            printf("ktls TX salt---------- %x \n", crypto_info->salt[i]);
        }
        for (int i = 0; i < 8; i++) {
            printf("ktls TX implicit_iv---------- %x \n", crypto_info->iv[i]);
        }
        for (int i = 0; i < 8; i++) {
            printf("ktls TX seq---------- %x \n", crypto_info->rec_seq[i]);
        }
        for (int i = 0; i < 16; i++) {
            printf("ktls TX key---------- %x \n", crypto_info->key[i]);
        }
        /* ================== */
        /* { */
        /*     printf("\n ktls client ===== \n"); */
        /*     for (int i = 0; i < client_traffic_key->size; i++) { */
        /*         printf("%x", client_traffic_key->data[i]); */
        /*     } */
        /*     printf("\n ktls ===== \n"); */
        /* } */
        /* { */
        /*     printf("\n ktls server ===== \n"); */
        /*     for (int i = 0; i < server_traffic_key->size; i++) { */
        /*         printf("%x", server_traffic_key->data[i]); */
        /*     } */
        /*     printf("\n ktls server ===== \n"); */
        /* } */
    }

    return S2N_RESULT_OK;
}

S2N_RESULT s2n_ktls_set_keys(struct s2n_connection *conn, s2n_ktls_mode ktls_mode, struct s2n_key_material *key_material)
{
    RESULT_ENSURE_REF(conn);
    RESULT_ENSURE_REF(key_material);

    RESULT_GUARD(s2n_ktls_validate(conn, ktls_mode));

    int fd;
    RESULT_GUARD(s2n_ktls_retrieve_file_descriptor(conn, ktls_mode, &fd));

    int tls_tx_rx_mode = 0;
    /* TODO Only AES_128_GCM for TLS 1.2 is supported at the moment. */
    struct s2n_tls12_crypto_info_aes_gcm_128 crypto_info = { 0 };
    RESULT_GUARD(s2n_ktls_init_aes128_gcm_crypto_info(conn, ktls_mode, key_material, &crypto_info, &tls_tx_rx_mode));

    /* Calls to setsockopt require a real socket, which is not used in unit tests. */
    RESULT_ENSURE(!disable_ktls_socket_config_for_testing, S2N_ERR_KTLS_DISABLED_FOR_TEST);

#if defined(__linux__)
    /* set keys */
    int ret_val = setsockopt(fd, S2N_SOL_TLS, tls_tx_rx_mode, &crypto_info, sizeof(crypto_info));
    if (ret_val < 0) {
        RESULT_BAIL(S2N_ERR_KTLS_ENABLE_CRYPTO);
    }
#endif

    return S2N_RESULT_OK;
}

static S2N_RESULT s2n_ktls_configure_socket(struct s2n_connection *conn, s2n_ktls_mode ktls_mode)
{
    RESULT_ENSURE_REF(conn);

    /* If already enabled then return success */
    if (ktls_mode == S2N_KTLS_MODE_SEND && conn->ktls_send_enabled) {
        RESULT_BAIL(S2N_ERR_KTLS_ALREADY_ENABLED);
    }
    if (ktls_mode == S2N_KTLS_MODE_RECV && conn->ktls_recv_enabled) {
        RESULT_BAIL(S2N_ERR_KTLS_ALREADY_ENABLED);
    }

    int fd = 0;
    RESULT_GUARD(s2n_ktls_retrieve_file_descriptor(conn, ktls_mode, &fd));

    /* Calls to setsockopt require a real socket, which is not used in unit tests. */
    RESULT_ENSURE(!disable_ktls_socket_config_for_testing, S2N_ERR_KTLS_DISABLED_FOR_TEST);

#if defined(__linux__)
    /* Enable 'tls' ULP for the socket. https://lwn.net/Articles/730207 */
    int ret = setsockopt(fd, S2N_SOL_TCP, S2N_TCP_ULP, S2N_TLS_ULP_NAME, S2N_TLS_ULP_NAME_SIZE);

    if (ret != 0) {
        /* EEXIST: https://man7.org/linux/man-pages/man3/errno.3.html
         *
         * TCP_ULP has already been enabled on the socket so the operation is a noop.
         * Since its possible to call this twice, once for TX and once for RX, consider
         * the noop a success. */
        if (errno != EEXIST) {
            RESULT_BAIL(S2N_ERR_KTLS_ULP);
        }
    }

#endif

    return S2N_RESULT_OK;
}

S2N_RESULT s2n_ktls_configure_connection(struct s2n_connection *conn, s2n_ktls_mode ktls_mode)
{
    RESULT_ENSURE_REF(conn);

    struct s2n_key_material key_material = { 0 };
    RESULT_GUARD(s2n_prf_generate_key_material(conn, &key_material));

    /* config ktls socket */
    RESULT_GUARD(s2n_ktls_set_keys(conn, ktls_mode, &key_material));

    return S2N_RESULT_OK;
}

/*
 * Since kTLS is an optimization, it is possible to continue operation
 * by using userspace TLS if kTLS is not supported. Upon successfully
 * enabling kTLS, we set connection->ktls_send_enabled (and recv) to true.
 *
 * kTLS configuration errors are recoverable since calls to setsockopt are
 * non-destructive and its possible to fallback to userspace.
 */
int s2n_connection_ktls_enable_send(struct s2n_connection *conn)
{
    POSIX_ENSURE_REF(conn);

    if (!s2n_ktls_is_supported_on_platform()) {
        POSIX_BAIL(S2N_ERR_KTLS_UNSUPPORTED_PLATFORM);
    }

    POSIX_GUARD_RESULT(s2n_ktls_validate(conn, S2N_KTLS_MODE_SEND));

    POSIX_GUARD_RESULT(s2n_ktls_configure_socket(conn, S2N_KTLS_MODE_SEND));
    POSIX_GUARD_RESULT(s2n_ktls_configure_connection(conn, S2N_KTLS_MODE_SEND));

    conn->ktls_send_enabled = true;
    return S2N_SUCCESS;
}

int s2n_connection_ktls_enable_recv(struct s2n_connection *conn)
{
    POSIX_ENSURE_REF(conn);

    if (!s2n_ktls_is_supported_on_platform()) {
        POSIX_BAIL(S2N_ERR_KTLS_UNSUPPORTED_PLATFORM);
    }

    POSIX_GUARD_RESULT(s2n_ktls_validate(conn, S2N_KTLS_MODE_RECV));

    POSIX_GUARD_RESULT(s2n_ktls_configure_socket(conn, S2N_KTLS_MODE_RECV));
    POSIX_GUARD_RESULT(s2n_ktls_configure_connection(conn, S2N_KTLS_MODE_RECV));

    conn->ktls_recv_enabled = true;
    return S2N_SUCCESS;
}

/* Use for testing only.
 *
 * This function disables the setsockopt call to enable ULP. Calls to setsockopt
 * require a real socket, which is not used in unit tests.
 */
S2N_RESULT s2n_disable_ktls_socket_config_for_testing(void)
{
    RESULT_ENSURE(s2n_in_unit_test(), S2N_ERR_NOT_IN_UNIT_TEST);

    disable_ktls_socket_config_for_testing = true;

    return S2N_RESULT_OK;
}

bool s2n_connection_is_ktls_enabled(struct s2n_connection *conn, s2n_ktls_mode ktls_mode)
{
    switch (ktls_mode) {
        case S2N_KTLS_MODE_RECV:
            return conn->ktls_recv_enabled;
            break;
        case S2N_KTLS_MODE_SEND:
            return conn->ktls_send_enabled;
            break;
    }
    return false;
}
