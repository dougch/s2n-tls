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

#include <stdint.h>

#include "stuffer/s2n_stuffer.h"
#include "tls/s2n_connection.h"

#define S2N_CLOSED_FD -1

extern const struct s2n_ecc_preferences ecc_preferences_for_retry;
extern const struct s2n_security_policy security_policy_test_tls13_retry;

/* Hex methods for testing */
S2N_RESULT s2n_stuffer_alloc_from_hex(struct s2n_stuffer *stuffer, const char *str);
/* Unlike other hex methods, the hex string read here may include spaces.
 * This is useful for hex strings with odd whitespace for readability purposes. */
S2N_RESULT s2n_blob_alloc_from_hex_with_whitespace(struct s2n_blob *bytes_out, const char *str);

void s2n_print_connection(struct s2n_connection *conn, const char *marker);

int s2n_connection_set_io_stuffers(struct s2n_stuffer *input, struct s2n_stuffer *output, struct s2n_connection *conn);
int s2n_connection_set_recv_io_stuffer(struct s2n_stuffer *input, struct s2n_connection *conn);
int s2n_connection_set_send_io_stuffer(struct s2n_stuffer *output, struct s2n_connection *conn);

struct s2n_test_io_stuffer_pair {
    struct s2n_stuffer client_in;
    struct s2n_stuffer server_in;
};
S2N_RESULT s2n_io_stuffer_pair_init(struct s2n_test_io_stuffer_pair *io_pair);
S2N_CLEANUP_RESULT s2n_io_stuffer_pair_free(struct s2n_test_io_stuffer_pair *io_pair);
S2N_RESULT s2n_connections_set_io_stuffer_pair(struct s2n_connection *client, struct s2n_connection *server,
        struct s2n_test_io_stuffer_pair *io_pair);

struct s2n_test_io_pair {
    int client;
    int server;
};
int s2n_io_pair_init(struct s2n_test_io_pair *io_pair);
int s2n_io_pair_init_non_blocking(struct s2n_test_io_pair *io_pair);
int s2n_io_pair_close(struct s2n_test_io_pair *io_pair);
int s2n_io_pair_close_one_end(struct s2n_test_io_pair *io_pair, int mode_to_close);
int s2n_io_pair_shutdown_one_end(struct s2n_test_io_pair *io_pair, int mode_to_close, int how);

int s2n_connection_set_io_pair(struct s2n_connection *conn, struct s2n_test_io_pair *io_pair);
int s2n_connections_set_io_pair(struct s2n_connection *client, struct s2n_connection *server,
        struct s2n_test_io_pair *io_pair);

int s2n_fd_set_blocking(int fd);
int s2n_fd_set_non_blocking(int fd);

int s2n_set_connection_hello_retry_flags(struct s2n_connection *conn);
int s2n_connection_mark_extension_received(struct s2n_connection *conn, uint16_t iana_value);
int s2n_connection_allow_response_extension(struct s2n_connection *conn, uint16_t iana_value);
int s2n_connection_allow_all_response_extensions(struct s2n_connection *conn);
int s2n_connection_set_all_protocol_versions(struct s2n_connection *conn, uint8_t version);
S2N_RESULT s2n_set_all_mutually_supported_groups(struct s2n_connection *conn);
S2N_RESULT s2n_skip_handshake(struct s2n_connection *conn);
S2N_RESULT s2n_connection_set_test_message_type(struct s2n_connection *conn, message_type_t expected_message_type);

S2N_RESULT s2n_connection_set_secrets(struct s2n_connection *conn);

S2N_RESULT s2n_config_mock_wall_clock(struct s2n_config *config, uint64_t *test_time_in_ns);

struct s2n_psk *s2n_test_psk_new(struct s2n_connection *conn);
S2N_RESULT s2n_append_test_psk_with_early_data(struct s2n_connection *conn, uint32_t max_early_data,
        const struct s2n_cipher_suite *cipher_suite);
S2N_RESULT s2n_append_test_chosen_psk_with_early_data(struct s2n_connection *conn, uint32_t max_early_data,
        const struct s2n_cipher_suite *cipher_suite);

S2N_RESULT s2n_connection_set_test_transcript_hash(struct s2n_connection *conn,
        message_type_t message_type, const struct s2n_blob *digest);
S2N_RESULT s2n_connection_set_test_early_secret(struct s2n_connection *conn, const struct s2n_blob *early_secret);
S2N_RESULT s2n_connection_set_test_handshake_secret(struct s2n_connection *conn, const struct s2n_blob *handshake_secret);
S2N_RESULT s2n_connection_set_test_master_secret(struct s2n_connection *conn, const struct s2n_blob *master_secret);

#define S2N_MAX_TEST_PEM_SIZE        12000
#define S2N_MAX_TEST_PEM_PATH_LENGTH 512

/* These paths assume that the unit tests are run from inside the unit/ directory.
 * Absolute paths will be needed if test directories go to deeper levels.
 */
#define S2N_RSA_2048_PKCS8_CERT_CHAIN        "../pems/rsa_2048_pkcs8_cert.pem"
#define S2N_RSA_2048_PKCS1_CERT_CHAIN        "../pems/rsa_2048_pkcs1_cert.pem"
#define S2N_RSA_2048_PKCS1_SHA256_CERT_CHAIN "../pems/permutations/rsae_pkcs_2048_sha256/server-chain.pem"
#define S2N_RSA_2048_PKCS1_SHA256_CERT_KEY   "../pems/permutations/rsae_pkcs_2048_sha256/server-key.pem"

#define S2N_RSA_2048_PKCS1_LEAF_CERT    "../pems/rsa_2048_pkcs1_leaf.pem"
#define S2N_ECDSA_P256_PKCS1_CERT_CHAIN "../pems/ecdsa_p256_pkcs1_cert.pem"
#define S2N_ECDSA_P384_PKCS1_CERT_CHAIN "../pems/ecdsa_p384_pkcs1_cert.pem"
#define S2N_ECDSA_P512_CERT_CHAIN       "../pems/ecdsa_p521_cert.pem"
#define S2N_RSA_CERT_CHAIN_CRLF         "../pems/rsa_2048_pkcs1_cert_crlf.pem"
#define S2N_RSA_KEY_CRLF                "../pems/rsa_2048_pkcs1_key_crlf.pem"
#define S2N_ECDSA_P256_PKCS1_KEY        "../pems/ecdsa_p256_pkcs1_key.pem"
#define S2N_ECDSA_P384_PKCS1_KEY        "../pems/ecdsa_p384_pkcs1_key.pem"
#define S2N_ECDSA_P512_KEY              "../pems/ecdsa_p521_key.pem"
#define S2N_RSA_2048_PKCS1_KEY          "../pems/rsa_2048_pkcs1_key.pem"
#define S2N_RSA_2048_PKCS8_KEY          "../pems/rsa_2048_pkcs8_key.pem"

#define S2N_RSA_PSS_2048_SHA256_CA_KEY    "../pems/rsa_pss_2048_sha256_CA_key.pem"
#define S2N_RSA_PSS_2048_SHA256_CA_CERT   "../pems/rsa_pss_2048_sha256_CA_cert.pem"
#define S2N_RSA_PSS_2048_SHA256_LEAF_KEY  "../pems/rsa_pss_2048_sha256_leaf_key.pem"
#define S2N_RSA_PSS_2048_SHA256_LEAF_CERT "../pems/rsa_pss_2048_sha256_leaf_cert.pem"

#define S2N_MLDSA87_KEY  "../pems/mldsa/ML-DSA-87-seed.priv"
#define S2N_MLDSA87_CERT "../pems/mldsa/ML-DSA-87.crt"

#define S2N_RSA_2048_SHA256_CLIENT_CERT "../pems/rsa_2048_sha256_client_cert.pem"

#define S2N_RSA_2048_SHA256_NO_DNS_SANS_CERT "../pems/rsa_2048_sha256_no_dns_sans_cert.pem"
#define S2N_RSA_2048_SHA256_WILDCARD_CERT    "../pems/rsa_2048_sha256_wildcard_cert.pem"
#define S2N_RSA_2048_SHA256_WILDCARD_KEY     "../pems/rsa_2048_sha256_wildcard_key.pem"

#define S2N_RSA_2048_SHA256_URI_SANS_CERT "../pems/rsa_2048_sha256_uri_sans_cert.pem"

/* "Strangely" formatted PEMs that should still parse successfully */
#define S2N_LEAF_WHITESPACE_CERT_CHAIN         "../pems/rsa_2048_leaf_whitespace_cert.pem"
#define S2N_INTERMEDIATE_WHITESPACE_CERT_CHAIN "../pems/rsa_2048_intermediate_whitespace_cert.pem"
#define S2N_ROOT_WHITESPACE_CERT_CHAIN         "../pems/rsa_2048_root_whitespace_cert.pem"
#define S2N_TRAILING_WHITESPACE_CERT_CHAIN     "../pems/rsa_2048_trailing_whitespace_cert.pem"
#define S2N_LEADING_COMMENT_TEXT_CERT_CHAIN    "../pems/rsa_2048_leading_comment_text_cert.pem"
#define S2N_LONG_BASE64_LINES_CERT_CHAIN       "../pems/rsa_2048_varying_base64_len_cert.pem"
/* Missing line endings between PEM encapsulation boundaries */
#define S2N_MISSING_LINE_ENDINGS_CERT_CHAIN "../pems/rsa_2048_missing_line_endings_cert.pem"

/* PEMs with invalid timestamp fields */
#define S2N_EXPIRED_CERT_CHAIN       "../pems/rsa_2048_expired_cert.pem"
#define S2N_EXPIRED_KEY              "../pems/rsa_2048_expired_key.pem"
#define S2N_NOT_YET_VALID_CERT_CHAIN "../pems/rsa_2048_not_yet_valid_cert.pem"
#define S2N_NOT_YET_VALID_KEY        "../pems/rsa_2048_not_yet_valid_key.pem"

/* Illegally formatted PEMs */
#define S2N_INVALID_HEADER_CERT_CHAIN  "../pems/rsa_2048_invalid_header_cert.pem"
#define S2N_INVALID_TRAILER_CERT_CHAIN "../pems/rsa_2048_invalid_trailer_cert.pem"
#define S2N_UNKNOWN_KEYWORD_CERT_CHAIN "../pems/rsa_2048_unknown_keyword_cert.pem"
#define S2N_INVALID_HEADER_KEY         "../pems/rsa_2048_invalid_header_key.pem"
#define S2N_INVALID_TRAILER_KEY        "../pems/rsa_2048_invalid_trailer_key.pem"
#define S2N_UNKNOWN_KEYWORD_KEY        "../pems/rsa_2048_unknown_keyword_key.pem"
#define S2N_WEIRD_DASHES_CERT_CHAIN    "../pems/rsa_2048_weird_dashes_cert.pem"
#define S2N_NO_DASHES_CERT_CHAIN       "../pems/rsa_2048_no_dashes_cert.pem"

/* Certificate with unusual curve not supported by awslc */
#define S2N_BRAINPOOL_CURVE_CERT "../pems/ecdsa_brainpoolP512t1_cert.pem"

/* OCSP Stapled Response Testing files */
#define S2N_OCSP_SERVER_CERT              "../pems/ocsp/server_cert.pem"
#define S2N_OCSP_SERVER_CERT_EARLY_EXPIRE "../pems/ocsp/server_cert_early_expire.pem"
#define S2N_OCSP_SERVER_ECDSA_CERT        "../pems/ocsp/server_ecdsa_cert.pem"

#define S2N_OCSP_SERVER_KEY                  "../pems/ocsp/server_key.pem"
#define S2N_OCSP_CA_CERT                     "../pems/ocsp/ca_cert.pem"
#define S2N_OCSP_CA_KEY                      "../pems/ocsp/ca_key.pem"
#define S2N_OCSP_RESPONSE_DER                "../pems/ocsp/ocsp_response.der"
#define S2N_OCSP_RESPONSE_EARLY_EXPIRE_DER   "../pems/ocsp/ocsp_response_early_expire.der"
#define S2N_OCSP_RESPONSE_NO_NEXT_UPDATE_DER "../pems/ocsp/ocsp_response_no_next_update.der"
#define S2N_OCSP_RESPONSE_REVOKED_DER        "../pems/ocsp/ocsp_response_revoked.der"
#define S2N_OCSP_RESPONSE_WRONG_SIGNER_DER   "../pems/ocsp/ocsp_response_wrong_signer.der"
#define S2N_OCSP_RESPONSE_CERT               "../pems/ocsp/ocsp_cert.pem"

#define S2N_ALLIGATOR_SAN_CERT  "../pems/sni/alligator_cert.pem"
#define S2N_ALLIGATOR_SAN_KEY   "../pems/sni/alligator_key.pem"
#define S2N_IP_V6_LO_RSA_CERT   "../pems/sni/ip_v6_lo_rsa_cert.pem"
#define S2N_IP_V6_LO_RSA_KEY    "../pems/sni/ip_v6_lo_rsa_key.pem"
#define S2N_WITHOUT_CN_RSA_CERT "../pems/sni/without_cn_rsa_cert.pem"
#define S2N_WITHOUT_CN_RSA_KEY  "../pems/sni/without_cn_rsa_key.pem"

#define S2N_DHPARAMS_2048 "../pems/dhparams_2048.pem"

#define S2N_ONE_TRAILING_BYTE_CERT_BIN  "../pems/one_trailing_byte_cert.bin"
#define S2N_FOUR_TRAILING_BYTE_CERT_BIN "../pems/four_trailing_byte_cert.bin"

/* This is a certificate with a legacy SHA-1 signature on the root certificate. This is used to prove
 * that our certificate validation code does not fail a root certificate signed with SHA-1. */
#define S2N_SHA1_ROOT_SIGNATURE_CA_CERT "../pems/rsa_1024_sha1_CA_cert.pem"

/* The leaf and intermediate have larger key sizes than the root. */
#define S2N_MIXED_CHAIN_CERTS "../pems/mixed_chains/ecdsa/server-chain.pem"
#define S2N_MIXED_CHAIN_KEY   "../pems/mixed_chains/ecdsa/server-key.pem"
#define S2N_MIXED_CHAIN_CA    "../pems/mixed_chains/ecdsa/ca-cert.pem"

#define S2N_TEST_TRUST_STORE "../pems/trust-store/ca-bundle.crt"

#define S2N_DEFAULT_TEST_CERT_CHAIN  S2N_RSA_2048_PKCS1_SHA256_CERT_CHAIN
#define S2N_DEFAULT_TEST_PRIVATE_KEY S2N_RSA_2048_PKCS1_SHA256_CERT_KEY

#define S2N_DEFAULT_ECDSA_TEST_CERT_CHAIN  S2N_ECDSA_P384_PKCS1_CERT_CHAIN
#define S2N_DEFAULT_ECDSA_TEST_PRIVATE_KEY S2N_ECDSA_P384_PKCS1_KEY

#define S2N_DEFAULT_TEST_DHPARAMS S2N_DHPARAMS_2048

/* Read a cert given a path into pem_out */
int s2n_read_test_pem(const char *pem_path, char *pem_out, long int max_size);
int s2n_read_test_pem_and_len(const char *pem_path, uint8_t *pem_out, uint32_t *pem_len, long int max_size);
int s2n_test_cert_chain_and_key_new(struct s2n_cert_chain_and_key **chain_and_key,
        const char *cert_chain_file, const char *private_key_file);
/**
 * load the `server-cert.pem` for the appropriate permutation
 * @param type indicates an `ec`, `rsae` or `rsapss` key type
 * @param signature indicates an `ecdsa`, `pkcs`, or `pss` signature
 * @param size indicates an rsa cert of `2048`, `3072`, or `4096` or an ecdsa
 * cert with `p256` or `p384`
 * @param digest indicates the certificate signature digest of `sha256` or
 * `sha384`
 * 
 * @note The OpenSSL FIPS module requires the key size for RSA certs to be greater than or equal to 1024 bits.
 * See https://github.com/aws/s2n-tls/issues/4651 for more information.
 */
int s2n_test_cert_permutation_load_server_chain(struct s2n_cert_chain_and_key **chain_and_key,
        const char *type, const char *siganture, const char *size, const char *digest);

int s2n_test_cert_permutation_get_ca_path(char *output, const char *type, const char *siganture,
        const char *size, const char *digest);
S2N_RESULT s2n_test_cert_permutation_get_server_chain_path(char *output, const char *type,
        const char *siganture, const char *size, const char *digest);

S2N_RESULT s2n_test_cert_chain_data_from_pem(struct s2n_connection *conn, const char *pem_path,
        struct s2n_stuffer *cert_chain_stuffer);
S2N_RESULT s2n_test_cert_chain_data_from_pem_data(struct s2n_connection *conn, uint8_t *pem_data, uint32_t pem_data_len,
        struct s2n_stuffer *cert_chain_stuffer);

int s2n_negotiate_test_server_and_client(struct s2n_connection *server_conn, struct s2n_connection *client_conn);
S2N_RESULT s2n_negotiate_test_server_and_client_until_message(struct s2n_connection *server_conn,
        struct s2n_connection *client_conn, message_type_t message_type);
int s2n_shutdown_test_server_and_client(struct s2n_connection *server_conn, struct s2n_connection *client_conn);
S2N_RESULT s2n_negotiate_test_server_and_client_with_early_data(struct s2n_connection *server_conn,
        struct s2n_connection *client_conn, struct s2n_blob *early_data_to_send, struct s2n_blob *early_data_received);
S2N_RESULT s2n_send_and_recv_test(struct s2n_connection *send_conn, struct s2n_connection *recv_conn);

/* Testing only with easily constructed contiguous data buffers could hide errors.
 * We should use iovecs where every buffer is allocated separately.
 * These test methods construct separate io buffers from one contiguous buffer.
 */
struct s2n_test_iovecs {
    struct iovec *iovecs;
    size_t iovecs_count;
};
S2N_RESULT s2n_test_new_iovecs(struct s2n_test_iovecs *iovecs,
        struct s2n_blob *data, const size_t *lens, size_t lens_count);
S2N_CLEANUP_RESULT s2n_test_iovecs_free(struct s2n_test_iovecs *in);

/* Expects 2 s2n_blobs to be equal (same size and contents) */
#define S2N_BLOB_EXPECT_EQUAL(blob1, blob2)                         \
    do {                                                            \
        EXPECT_EQUAL(blob1.size, blob2.size);                       \
        EXPECT_BYTEARRAY_EQUAL(blob1.data, blob2.data, blob1.size); \
    } while (0)

/* Expects data of type in stuffer, where type is uint32, uint64 etc.. */
#define S2N_STUFFER_READ_EXPECT_EQUAL(stuffer, expected, type)    \
    do {                                                          \
        type##_t value;                                           \
        EXPECT_SUCCESS(s2n_stuffer_read_##type(stuffer, &value)); \
        EXPECT_EQUAL(value, expected);                            \
    } while (0)

/* Expects written length in stuffer */
#define S2N_STUFFER_LENGTH_WRITTEN_EXPECT_EQUAL(stuffer, bytes) \
    do {                                                        \
        EXPECT_SUCCESS(s2n_stuffer_skip_read(stuffer, bytes));  \
        EXPECT_EQUAL(s2n_stuffer_data_available(stuffer), 0);   \
    } while (0)

int s2n_public_ecc_keys_are_equal(struct s2n_ecc_evp_params *params_1, struct s2n_ecc_evp_params *params_2);

extern const s2n_parsed_extension EMPTY_PARSED_EXTENSIONS[S2N_PARSED_EXTENSIONS_COUNT];
#define EXPECT_PARSED_EXTENSION_LIST_EMPTY(list)     EXPECT_BYTEARRAY_EQUAL(list.parsed_extensions, EMPTY_PARSED_EXTENSIONS, sizeof(EMPTY_PARSED_EXTENSIONS))
#define EXPECT_PARSED_EXTENSION_LIST_NOT_EMPTY(list) EXPECT_BYTEARRAY_NOT_EQUAL(list.parsed_extensions, EMPTY_PARSED_EXTENSIONS, sizeof(EMPTY_PARSED_EXTENSIONS))

int s2n_kem_recv_public_key_fuzz_test(const uint8_t *buf, size_t len, struct s2n_kem_params *kem_params);
int s2n_kem_recv_ciphertext_fuzz_test(const uint8_t *buf, size_t len, struct s2n_kem_params *kem_params);
int s2n_kem_recv_ciphertext_fuzz_test_init(const char *kat_file_path, struct s2n_kem_params *kem_params);

S2N_RESULT s2n_resumption_test_ticket_key_setup(struct s2n_config *config);

#define S2N_BLOB_FROM_HEX(name, hex) S2N_CHECKED_BLOB_FROM_HEX(name, POSIX_GUARD_RESULT, hex)
#define S2N_CHECKED_BLOB_FROM_HEX(name, check, hex)        \
    DEFER_CLEANUP(struct s2n_blob name = { 0 }, s2n_free); \
    check(s2n_blob_alloc_from_hex_with_whitespace(&name, (const char *) hex));

bool s2n_is_seccomp_supported();
S2N_RESULT s2n_seccomp_init();
