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

#include "s2n_test.h"
#include "testlib/s2n_testlib.h"
#include "tls/s2n_ktls.h"

S2N_RESULT s2n_disable_ktls_socket_config_for_testing(void);
S2N_RESULT s2n_ktls_init_aes128_gcm_crypto_info(struct s2n_connection *conn, s2n_ktls_mode ktls_mode,
        struct s2n_key_material *key_material, struct s2n_tls12_crypto_info_aes_gcm_128 *crypto_info, int *tls_mode);
S2N_RESULT s2n_ktls_set_keys(struct s2n_connection *conn, s2n_ktls_mode ktls_mode, struct s2n_key_material *key_material);

S2N_RESULT helper_generate_test_data(struct s2n_blob *test_data)
{
    struct s2n_stuffer test_data_stuffer = { 0 };
    RESULT_GUARD_POSIX(s2n_stuffer_init(&test_data_stuffer, test_data));
    for (int i = 0; i < S2N_MAX_KEY_BLOCK_LEN; i++) {
        RESULT_GUARD_POSIX(s2n_stuffer_write_uint8(&test_data_stuffer, i));
    }
    return S2N_RESULT_OK;
}

int main(int argc, char **argv)
{
    BEGIN_TEST();

    EXPECT_OK(s2n_disable_ktls_socket_config_for_testing());

    uint8_t test_data[S2N_MAX_KEY_BLOCK_LEN] = { 0 };
    struct s2n_blob test_data_blob = { 0 };
    EXPECT_SUCCESS(s2n_blob_init(&test_data_blob, test_data, sizeof(test_data)));
    EXPECT_OK(helper_generate_test_data(&test_data_blob));

    if (s2n_ktls_is_supported_on_platform()) {
        /* s2n_ktls_create_aes128_gcm_crypto_info */
        {
            DEFER_CLEANUP(struct s2n_cert_chain_and_key * chain_and_key,
                    s2n_cert_chain_and_key_ptr_free);
            DEFER_CLEANUP(struct s2n_connection *server_conn = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_connection *client_conn = s2n_connection_new(S2N_CLIENT),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_config *config = s2n_config_new(),
                    s2n_config_ptr_free);

            /* setup config */
            EXPECT_SUCCESS(s2n_test_cert_chain_and_key_new(&chain_and_key,
                    S2N_DEFAULT_TEST_CERT_CHAIN, S2N_DEFAULT_TEST_PRIVATE_KEY));
            EXPECT_SUCCESS(s2n_config_add_cert_chain_and_key_to_store(config, chain_and_key));
            EXPECT_SUCCESS(s2n_config_disable_x509_verification(config));
            EXPECT_SUCCESS(s2n_config_set_cipher_preferences(config, "default"));
            EXPECT_SUCCESS(s2n_connection_set_config(client_conn, config));
            EXPECT_SUCCESS(s2n_connection_set_config(server_conn, config));

            /* setup IO */
            struct s2n_test_io_pair io_pair = { 0 };
            EXPECT_SUCCESS(s2n_io_pair_init_non_blocking(&io_pair));
            EXPECT_SUCCESS(s2n_connections_set_io_pair(client_conn, server_conn, &io_pair));

            EXPECT_SUCCESS(s2n_negotiate_test_server_and_client(server_conn, client_conn));
            EXPECT_EQUAL(server_conn->actual_protocol_version, S2N_TLS12);

            /* copy test data to key_material */
            struct s2n_key_material key_material = { 0 };
            EXPECT_OK(s2n_key_material_init(&key_material, server_conn));
            POSIX_CHECKED_MEMCPY(key_material.key_block, test_data, s2n_array_len(key_material.key_block));

            POSIX_ENSURE_EQ(key_material.client_key.size, S2N_TLS_AES_128_GCM_KEY_LEN);
            POSIX_ENSURE_EQ(key_material.server_key.size, S2N_TLS_AES_128_GCM_KEY_LEN);

            int tls_tx_rx_mode = 0;
            int ktls_mode = S2N_KTLS_MODE_SEND;
            struct s2n_tls12_crypto_info_aes_gcm_128 crypto_info = { 0 };

            /* server should send with its own keys */
            EXPECT_OK(s2n_ktls_init_aes128_gcm_crypto_info(server_conn, ktls_mode, &key_material, &crypto_info, &tls_tx_rx_mode));
            EXPECT_EQUAL(memcmp(key_material.server_key.data, crypto_info.key, key_material.server_key.size), 0);
            EXPECT_EQUAL(tls_tx_rx_mode, S2N_TLS_TX);
            /* client should send with its own keys */
            EXPECT_OK(s2n_ktls_init_aes128_gcm_crypto_info(client_conn, ktls_mode, &key_material, &crypto_info, &tls_tx_rx_mode));
            EXPECT_EQUAL(tls_tx_rx_mode, S2N_TLS_TX);
            EXPECT_EQUAL(memcmp(key_material.client_key.data, crypto_info.key, key_material.client_key.size), 0);

            ktls_mode = S2N_KTLS_MODE_RECV;
            /* server should recv with its peers keys */
            EXPECT_OK(s2n_ktls_init_aes128_gcm_crypto_info(server_conn, ktls_mode, &key_material, &crypto_info, &tls_tx_rx_mode));
            EXPECT_EQUAL(tls_tx_rx_mode, S2N_TLS_RX);
            EXPECT_EQUAL(memcmp(key_material.client_key.data, crypto_info.key, key_material.client_key.size), 0);
            /* client should recv with its peers keys */
            EXPECT_OK(s2n_ktls_init_aes128_gcm_crypto_info(client_conn, ktls_mode, &key_material, &crypto_info, &tls_tx_rx_mode));
            EXPECT_EQUAL(tls_tx_rx_mode, S2N_TLS_RX);
            EXPECT_EQUAL(memcmp(key_material.server_key.data, crypto_info.key, key_material.server_key.size), 0);

            EXPECT_SUCCESS(s2n_io_pair_close(&io_pair));
        };

        /* s2n_ktls_set_keys */
        {
            DEFER_CLEANUP(struct s2n_cert_chain_and_key * chain_and_key,
                    s2n_cert_chain_and_key_ptr_free);
            DEFER_CLEANUP(struct s2n_connection *server_conn = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_connection *client_conn = s2n_connection_new(S2N_CLIENT),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_config *config = s2n_config_new(),
                    s2n_config_ptr_free);

            /* setup config */
            EXPECT_SUCCESS(s2n_test_cert_chain_and_key_new(&chain_and_key,
                    S2N_DEFAULT_TEST_CERT_CHAIN, S2N_DEFAULT_TEST_PRIVATE_KEY));
            EXPECT_SUCCESS(s2n_config_add_cert_chain_and_key_to_store(config, chain_and_key));
            EXPECT_SUCCESS(s2n_config_disable_x509_verification(config));
            EXPECT_SUCCESS(s2n_config_set_cipher_preferences(config, "default"));
            EXPECT_SUCCESS(s2n_connection_set_config(client_conn, config));
            EXPECT_SUCCESS(s2n_connection_set_config(server_conn, config));

            /* setup IO */
            struct s2n_test_io_pair io_pair = { 0 };
            EXPECT_SUCCESS(s2n_io_pair_init_non_blocking(&io_pair));
            EXPECT_SUCCESS(s2n_connections_set_io_pair(client_conn, server_conn, &io_pair));

            EXPECT_SUCCESS(s2n_negotiate_test_server_and_client(server_conn, client_conn));
            EXPECT_EQUAL(server_conn->actual_protocol_version, S2N_TLS12);

            /* copy test data to key_material */
            struct s2n_key_material key_material = { 0 };
            EXPECT_OK(s2n_key_material_init(&key_material, server_conn));
            POSIX_CHECKED_MEMCPY(key_material.key_block, test_data, s2n_array_len(key_material.key_block));

            /* call set keys with send mode */
            int ktls_mode = S2N_KTLS_MODE_SEND;
            EXPECT_ERROR_WITH_ERRNO(s2n_ktls_set_keys(server_conn, ktls_mode, &key_material), S2N_ERR_KTLS_DISABLED_FOR_TEST);

            /* call set keys with recv mode */
            ktls_mode = S2N_KTLS_MODE_RECV;
            EXPECT_ERROR_WITH_ERRNO(s2n_ktls_set_keys(server_conn, ktls_mode, &key_material), S2N_ERR_KTLS_DISABLED_FOR_TEST);

            EXPECT_SUCCESS(s2n_io_pair_close(&io_pair));
        }
    }

    END_TEST();
}
