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

#include "error/s2n_errno.h"
#include "s2n_test.h"
#include "testlib/s2n_testlib.h"
#include "utils/s2n_random.h"

S2N_RESULT s2n_ktls_mock_io_for_testing(void);

S2N_RESULT s2n_test_init_ktls_conn_and_stuffer_io(struct s2n_connection *server, struct s2n_connection *client,
        struct s2n_test_ktls_io_pair *io_pair)
{
    server->actual_protocol_version = S2N_TLS12;
    server->ktls_recv_enabled = true;
    server->ktls_send_enabled = true;
    client->actual_protocol_version = S2N_TLS12;
    client->ktls_recv_enabled = true;
    client->ktls_send_enabled = true;

    RESULT_GUARD_POSIX(s2n_stuffer_growable_alloc(&io_pair->client_in.data, 0));
    RESULT_GUARD_POSIX(s2n_stuffer_growable_alloc(&io_pair->client_in.ancillary, 0));
    RESULT_GUARD_POSIX(s2n_stuffer_growable_alloc(&io_pair->server_in.data, 0));
    RESULT_GUARD_POSIX(s2n_stuffer_growable_alloc(&io_pair->server_in.ancillary, 0));

    server->send_io_context = &io_pair->client_in;
    client->recv_io_context = &io_pair->client_in;

    server->recv_io_context = &io_pair->server_in;
    client->send_io_context = &io_pair->server_in;

    return S2N_RESULT_OK;
}

int main(int argc, char **argv)
{
    BEGIN_TEST();

    EXPECT_OK(s2n_ktls_mock_io_for_testing());

    /* prepare test data */
    uint8_t test_data[S2N_MAX_KEY_BLOCK_LEN] = { 0 };
    struct s2n_blob test_data_blob = { 0 };
    EXPECT_SUCCESS(s2n_blob_init(&test_data_blob, test_data, sizeof(test_data)));
    EXPECT_OK(s2n_get_public_random_data(&test_data_blob));

    /* Test mock IO */
    {
        /* Test that s2n_send writes to stuffer */
        {
            uint8_t io_buf[160] = { 0 };
            s2n_blocked_status blocked = S2N_NOT_BLOCKED;
            ssize_t size = 20;
            uint8_t record_type = TLS_APPLICATION_DATA;

            DEFER_CLEANUP(struct s2n_connection *server = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_connection *client = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_test_ktls_io_pair pair = { 0 },
                    s2n_ktls_io_pair_free);
            EXPECT_OK(s2n_test_init_ktls_conn_and_stuffer_io(server, client, &pair));

            /* s2n_send */
            pair.client_in.send_record_type = record_type;
            ssize_t send_ret = s2n_send(server, test_data, size, &blocked);
            EXPECT_EQUAL(send_ret, size);
            EXPECT_EQUAL(s2n_stuffer_data_available(&pair.client_in.data), size);
            EXPECT_EQUAL(s2n_stuffer_data_available(&pair.server_in.data), 0);

            /* verify ancillary data */
            EXPECT_EQUAL(s2n_stuffer_data_available(&pair.client_in.ancillary), 2);
            EXPECT_EQUAL(s2n_stuffer_data_available(&pair.server_in.ancillary), 0);
            uint8_t read_byte = 0;
            s2n_stuffer_read_uint8(&pair.client_in.ancillary, &read_byte);
            EXPECT_EQUAL(read_byte, record_type);
            s2n_stuffer_read_uint8(&pair.client_in.ancillary, &read_byte);
            EXPECT_EQUAL(read_byte, size);

            /* ensure s2n_send wrote to the stuffer */
            uint8_t *ptr = s2n_stuffer_raw_read(&pair.client_in.data, size);
            EXPECT_NOT_NULL(ptr);
            EXPECT_EQUAL(memcmp(test_data, ptr, size), 0);
        };

        /* Test s2n_recv reads from stuffer */
        {
            uint8_t io_buf[160] = { 0 };
            s2n_blocked_status blocked = S2N_NOT_BLOCKED;
            ssize_t size = 20;
            uint8_t record_type = TLS_APPLICATION_DATA;

            DEFER_CLEANUP(struct s2n_connection *server = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_connection *client = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_test_ktls_io_pair pair = { 0 },
                    s2n_ktls_io_pair_free);
            EXPECT_OK(s2n_test_init_ktls_conn_and_stuffer_io(server, client, &pair));

            /* set to 0 and insure that the read impl get the value from the ancillary stuffer */
            pair.client_in.send_record_type = 0;

            /* write ancillary data */
            EXPECT_SUCCESS(s2n_stuffer_write_uint8(&pair.client_in.ancillary, record_type));
            EXPECT_SUCCESS(s2n_stuffer_write_uint8(&pair.client_in.ancillary, size));

            EXPECT_SUCCESS(s2n_stuffer_write_bytes(&pair.client_in.data, test_data, size));
            EXPECT_EQUAL(s2n_stuffer_data_available(&pair.client_in.data), size);
            EXPECT_EQUAL(s2n_stuffer_data_available(&pair.server_in.data), 0);

            /* ensure s2n_send wrote to the stuffer */
            ssize_t recv_ret = s2n_recv(client, io_buf, size, &blocked);
            EXPECT_EQUAL(recv_ret, size);
            EXPECT_EQUAL(s2n_stuffer_data_available(&pair.client_in.data), 0);
            EXPECT_EQUAL(memcmp(test_data_blob.data, io_buf, size), 0);
        }
    }

    /* Test App data */
    {
        /* Test s2n_send and s2n_recv for client and server */
        {
            uint8_t io_buf[160] = { 0 };
            s2n_blocked_status blocked = S2N_NOT_BLOCKED;
            ssize_t size = 20;
            uint8_t record_type = TLS_APPLICATION_DATA;

            DEFER_CLEANUP(struct s2n_connection *server = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_connection *client = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_test_ktls_io_pair pair = { 0 },
                    s2n_ktls_io_pair_free);
            EXPECT_OK(s2n_test_init_ktls_conn_and_stuffer_io(server, client, &pair));

            /* server send, client recv */
            pair.client_in.send_record_type = record_type;
            ssize_t send_ret = s2n_send(server, test_data, size, &blocked);
            EXPECT_EQUAL(send_ret, size);
            ssize_t recv_ret = s2n_recv(client, io_buf, size, &blocked);
            EXPECT_EQUAL(recv_ret, size);
            EXPECT_EQUAL(memcmp(test_data_blob.data, io_buf, size), 0);

            /* client send, server recv */
            pair.server_in.send_record_type = record_type;
            send_ret = s2n_send(client, test_data, size, &blocked);
            EXPECT_EQUAL(send_ret, size);
            recv_ret = s2n_recv(server, io_buf, size, &blocked);
            EXPECT_EQUAL(recv_ret, size);
            EXPECT_EQUAL(memcmp(test_data_blob.data, io_buf, size), 0);
        }

        /* Test receiving more data than was sent */
        {
            uint8_t io_buf[160] = { 0 };
            s2n_blocked_status blocked = S2N_NOT_BLOCKED;
            ssize_t recv_size = 10;
            ssize_t send_size = 4;
            uint8_t record_type = TLS_APPLICATION_DATA;

            DEFER_CLEANUP(struct s2n_connection *server = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_connection *client = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_test_ktls_io_pair pair = { 0 },
                    s2n_ktls_io_pair_free);
            EXPECT_OK(s2n_test_init_ktls_conn_and_stuffer_io(server, client, &pair));

            pair.client_in.send_record_type = record_type;

            ssize_t send_ret = s2n_send(server, test_data, send_size, &blocked);
            EXPECT_EQUAL(send_ret, send_size);

            ssize_t recv_ret = s2n_recv(client, io_buf, recv_size, &blocked);
            EXPECT_EQUAL(recv_ret, send_size);
            EXPECT_EQUAL(memcmp(test_data_blob.data, io_buf, send_size), 0);
        };

        /* Test receiving less data than was sent */
        {
            uint8_t io_buf[160] = { 0 };
            s2n_blocked_status blocked = S2N_NOT_BLOCKED;
            ssize_t recv_size = 4;
            ssize_t send_size = 10;
            uint8_t record_type = TLS_APPLICATION_DATA;

            DEFER_CLEANUP(struct s2n_connection *server = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_connection *client = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_test_ktls_io_pair pair = { 0 },
                    s2n_ktls_io_pair_free);
            EXPECT_OK(s2n_test_init_ktls_conn_and_stuffer_io(server, client, &pair));
            pair.client_in.send_record_type = record_type;

            ssize_t send_ret = s2n_send(server, test_data, send_size, &blocked);
            EXPECT_EQUAL(send_ret, send_size);

            ssize_t recv_ret = s2n_recv(client, io_buf, recv_size, &blocked);
            EXPECT_EQUAL(recv_ret, recv_size);
            EXPECT_EQUAL(memcmp(test_data_blob.data, io_buf, recv_size), 0);

            /* offset buffer and receive remaining data */
            recv_ret = s2n_recv(client, io_buf + recv_size, send_size - recv_size, &blocked);
            EXPECT_EQUAL(recv_ret, send_size - recv_size);
            EXPECT_EQUAL(memcmp(test_data_blob.data, io_buf, send_size), 0);
        }

        /* Test blocked on read scenario */
        {
            uint8_t io_buf[160] = { 0 };
            s2n_blocked_status blocked = S2N_NOT_BLOCKED;
            ssize_t size = 20;

            DEFER_CLEANUP(struct s2n_connection *server = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_connection *client = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_test_ktls_io_pair pair = { 0 },
                    s2n_ktls_io_pair_free);
            EXPECT_OK(s2n_test_init_ktls_conn_and_stuffer_io(server, client, &pair));
            pair.client_in.send_record_type = TLS_APPLICATION_DATA;

            ssize_t send_ret = s2n_send(server, test_data, size, &blocked);
            EXPECT_EQUAL(send_ret, size);

            ssize_t recv_ret = s2n_recv(client, io_buf, size, &blocked);
            EXPECT_EQUAL(recv_ret, size);
            EXPECT_EQUAL(memcmp(test_data_blob.data, io_buf, size), 0);

            EXPECT_FAILURE_WITH_ERRNO(s2n_recv(client, io_buf, size, &blocked), S2N_ERR_IO_BLOCKED);
            EXPECT_EQUAL(blocked, S2N_BLOCKED_ON_READ);
        }

        /* Test blocked on write scenario */
        {
            uint8_t io_buf[160] = { 0 };
            s2n_blocked_status blocked = S2N_NOT_BLOCKED;
            ssize_t size = 1;

            DEFER_CLEANUP(struct s2n_connection *server = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_connection *client = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_test_ktls_io_pair pair = { 0 },
                    s2n_ktls_io_pair_free);
            EXPECT_OK(s2n_test_init_ktls_conn_and_stuffer_io(server, client, &pair));
            pair.client_in.data.growable = 0;
            pair.client_in.send_record_type = TLS_APPLICATION_DATA;

            /* blocked if there is no space in output buffer */
            EXPECT_EQUAL(s2n_stuffer_space_remaining(&pair.client_in.data), 0);
            EXPECT_FAILURE_WITH_ERRNO(s2n_send(server, test_data, size, &blocked), S2N_ERR_IO_BLOCKED);
            EXPECT_EQUAL(blocked, S2N_BLOCKED_ON_WRITE);

            /* alloc space and expect success */
            EXPECT_SUCCESS(s2n_stuffer_alloc(&pair.client_in.data, size));
            EXPECT_EQUAL(s2n_stuffer_space_remaining(&pair.client_in.data), 1);

            ssize_t send_ret = s2n_send(server, test_data, size, &blocked);
            EXPECT_EQUAL(send_ret, size);
            /* ensure s2n_send wrote to the stuffer */
            uint8_t *ptr = s2n_stuffer_raw_read(&pair.client_in.data, size);
            EXPECT_NOT_NULL(ptr);
            EXPECT_EQUAL(memcmp(test_data, ptr, size), 0);
            EXPECT_SUCCESS(s2n_stuffer_rewind_read(&pair.client_in.data, size));
        }

        /* Test receiving 2 coalesced app data records */
        {
            printf("---------------------\n\n");
            uint8_t io_buf[160] = { 0 };
            s2n_blocked_status blocked = S2N_NOT_BLOCKED;
            ssize_t recv_size = 8;
            ssize_t send_size = 5;
            uint8_t record_type = TLS_APPLICATION_DATA;

            DEFER_CLEANUP(struct s2n_connection *server = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_connection *client = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_test_ktls_io_pair pair = { 0 },
                    s2n_ktls_io_pair_free);
            EXPECT_OK(s2n_test_init_ktls_conn_and_stuffer_io(server, client, &pair));
            pair.client_in.send_record_type = record_type;

            /* send 5 */
            ssize_t send_ret = s2n_send(server, test_data, send_size, &blocked);
            EXPECT_EQUAL(send_ret, send_size);
            /* send 5 */
            send_ret = s2n_send(server, test_data + send_size, send_size, &blocked);
            EXPECT_EQUAL(send_ret, send_size);
            /* send 5 */
            send_ret = s2n_send(server, test_data + send_size + send_size, send_size, &blocked);
            EXPECT_EQUAL(send_ret, send_size);

            /* recv 8 */
            ssize_t recv_ret = s2n_recv(client, io_buf, recv_size, &blocked);
            EXPECT_EQUAL(recv_ret, recv_size);
            EXPECT_EQUAL(memcmp(test_data_blob.data, io_buf, recv_size), 0);

            /* recv 8 */
            recv_ret = s2n_recv(client, io_buf + recv_size, recv_size, &blocked);
            EXPECT_EQUAL(recv_ret, send_size * 3 - recv_size);
            EXPECT_EQUAL(memcmp(test_data_blob.data, io_buf, MIN(send_size * 3, recv_size * 2)), 0);
        }

        /* Test receiving coalesced app data and alert records */
        {
            uint8_t io_buf[160] = { 0 };
            s2n_blocked_status blocked = S2N_NOT_BLOCKED;
            ssize_t recv_size = 8;
            ssize_t send_size = 5;

            DEFER_CLEANUP(struct s2n_connection *server = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_connection *client = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_test_ktls_io_pair pair = { 0 },
                    s2n_ktls_io_pair_free);
            EXPECT_OK(s2n_test_init_ktls_conn_and_stuffer_io(server, client, &pair));
            pair.client_in.send_record_type = TLS_APPLICATION_DATA;

            ssize_t send_ret = s2n_send(server, test_data, send_size, &blocked);
            EXPECT_EQUAL(send_ret, send_size);

            pair.client_in.send_record_type = TLS_ALERT;
            EXPECT_SUCCESS(s2n_shutdown(server, &blocked));

            ssize_t recv_ret = s2n_recv(client, io_buf, recv_size, &blocked);
            EXPECT_EQUAL(recv_ret, send_size);
            EXPECT_EQUAL(memcmp(test_data_blob.data, io_buf, send_size), 0);

            /* offset buffer, attempting to read remaining data; but next record should be an alert */
            recv_ret = s2n_recv(client, io_buf + send_size, recv_size, &blocked);
            EXPECT_EQUAL(recv_ret, 0);
            EXPECT_EQUAL(memcmp(test_data_blob.data, io_buf, send_size), 0);
        }
    }

    /* Test alerts.
     * - sending warning is not supported with kTLS */
    {
        /* Test recv close_notify and sending one back */
        {
            uint8_t io_buf[160] = { 0 };
            s2n_blocked_status blocked = S2N_NOT_BLOCKED;
            uint8_t record_type = TLS_ALERT;
            uint8_t level = S2N_TLS_ALERT_LEVEL_WARNING;
            uint8_t code = S2N_TLS_ALERT_CLOSE_NOTIFY;

            DEFER_CLEANUP(struct s2n_connection *server = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_connection *client = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_test_ktls_io_pair pair = { 0 },
                    s2n_ktls_io_pair_free);
            EXPECT_OK(s2n_test_init_ktls_conn_and_stuffer_io(server, client, &pair));
            pair.client_in.send_record_type = record_type;

            /* server send alert */
            uint8_t alert[S2N_ALERT_LENGTH] = { level, code };
            EXPECT_SUCCESS(s2n_send(server, alert, S2N_ALERT_LENGTH, &blocked));

            EXPECT_FALSE(s2n_atomic_flag_test(&client->read_closed));
            EXPECT_FALSE(s2n_atomic_flag_test(&client->write_closed));
            EXPECT_FALSE(s2n_atomic_flag_test(&client->close_notify_received));
            EXPECT_FALSE(s2n_atomic_flag_test(&client->error_alert_received));

            /* client recv alert */
            ssize_t recv_ret = s2n_recv(client, io_buf, S2N_ALERT_LENGTH, &blocked);
            EXPECT_EQUAL(recv_ret, 0);

            EXPECT_TRUE(s2n_atomic_flag_test(&client->read_closed));
            EXPECT_FALSE(s2n_atomic_flag_test(&client->write_closed));
            EXPECT_TRUE(s2n_atomic_flag_test(&client->close_notify_received));
            EXPECT_FALSE(s2n_atomic_flag_test(&client->error_alert_received));

            /* subsequent client read/send indicate connection closed */
            recv_ret = s2n_recv(client, io_buf, S2N_ALERT_LENGTH, &blocked);
            EXPECT_EQUAL(recv_ret, 0);
            EXPECT_FAILURE_WITH_ERRNO(s2n_send(client, test_data, 20, &blocked), S2N_ERR_CLOSED);

            /* client send close_notify alert back */
            EXPECT_SUCCESS(s2n_shutdown(client, &blocked));
            EXPECT_EQUAL(s2n_stuffer_data_available(&pair.server_in.data), S2N_ALERT_LENGTH);
            uint8_t *ptr = s2n_stuffer_raw_read(&pair.server_in.data, S2N_ALERT_LENGTH);
            EXPECT_NOT_NULL(ptr);
            EXPECT_EQUAL(memcmp(alert, ptr, S2N_ALERT_LENGTH), 0);

            /* writer is also closed once the close_notify has been sent */
            EXPECT_TRUE(s2n_atomic_flag_test(&client->read_closed));
            EXPECT_TRUE(s2n_atomic_flag_test(&client->write_closed));
            EXPECT_TRUE(s2n_atomic_flag_test(&client->close_notify_received));
            EXPECT_FALSE(s2n_atomic_flag_test(&client->error_alert_received));
        };

        /* Test recv fatal alert */
        {
            uint8_t io_buf[160] = { 0 };
            s2n_blocked_status blocked = S2N_NOT_BLOCKED;
            uint8_t record_type = TLS_ALERT;
            uint8_t level = S2N_TLS_ALERT_LEVEL_FATAL;
            uint8_t code = S2N_TLS_ALERT_INTERNAL_ERROR;

            DEFER_CLEANUP(struct s2n_connection *server = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_connection *client = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_test_ktls_io_pair pair = { 0 },
                    s2n_ktls_io_pair_free);
            EXPECT_OK(s2n_test_init_ktls_conn_and_stuffer_io(server, client, &pair));
            pair.client_in.send_record_type = record_type;

            /* send alert */
            uint8_t alert[S2N_ALERT_LENGTH] = { level, code };
            EXPECT_SUCCESS(s2n_send(server, alert, S2N_ALERT_LENGTH, &blocked));

            /* ensure client state before alert */
            EXPECT_FALSE(s2n_atomic_flag_test(&client->read_closed));
            EXPECT_FALSE(s2n_atomic_flag_test(&client->write_closed));
            EXPECT_FALSE(s2n_atomic_flag_test(&client->close_notify_received));
            EXPECT_FALSE(s2n_atomic_flag_test(&client->error_alert_received));

            ssize_t recv_ret = s2n_recv(client, io_buf, S2N_ALERT_LENGTH, &blocked);
            EXPECT_FAILURE_WITH_ERRNO(recv_ret, S2N_ERR_ALERT);

            /* ensure connection state after alert */
            EXPECT_TRUE(s2n_atomic_flag_test(&client->read_closed));
            EXPECT_TRUE(s2n_atomic_flag_test(&client->write_closed));
            EXPECT_FALSE(s2n_atomic_flag_test(&client->close_notify_received));
            EXPECT_TRUE(s2n_atomic_flag_test(&client->error_alert_received));

            /* subsequent IO indicate closed */
            EXPECT_FAILURE_WITH_ERRNO(s2n_recv(client, test_data, 20, &blocked), S2N_ERR_CLOSED);
            EXPECT_FAILURE_WITH_ERRNO(s2n_send(client, test_data, 20, &blocked), S2N_ERR_CLOSED);

            /* unable to send anything after a fatal alert */
            EXPECT_SUCCESS(s2n_shutdown(client, &blocked));
            EXPECT_EQUAL(s2n_stuffer_data_available(&pair.client_in.data), 0);
        }

        /* Test sending close_notify via s2n_shutdown */
        {
            uint8_t io_buf[160] = { 0 };
            s2n_blocked_status blocked = S2N_NOT_BLOCKED;
            uint8_t record_type = TLS_ALERT;
            uint8_t level = S2N_TLS_ALERT_LEVEL_WARNING;
            uint8_t code = S2N_TLS_ALERT_CLOSE_NOTIFY;

            DEFER_CLEANUP(struct s2n_connection *server = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_connection *client = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_test_ktls_io_pair pair = { 0 },
                    s2n_ktls_io_pair_free);
            EXPECT_OK(s2n_test_init_ktls_conn_and_stuffer_io(server, client, &pair));
            pair.client_in.send_record_type = record_type;

            pair.client_in.send_record_type = record_type;

            /* ensure server state before alert */
            EXPECT_FALSE(s2n_atomic_flag_test(&server->read_closed));
            EXPECT_FALSE(s2n_atomic_flag_test(&server->write_closed));
            EXPECT_FALSE(s2n_atomic_flag_test(&server->close_notify_received));
            EXPECT_FALSE(s2n_atomic_flag_test(&server->error_alert_received));
            EXPECT_FALSE(server->alert_sent);

            /* write alert */
            EXPECT_SUCCESS(s2n_shutdown(server, &blocked));

            /* ensure connection state after alert */
            EXPECT_TRUE(s2n_atomic_flag_test(&server->read_closed));
            EXPECT_TRUE(s2n_atomic_flag_test(&server->write_closed));
            EXPECT_FALSE(s2n_atomic_flag_test(&server->close_notify_received));
            EXPECT_FALSE(s2n_atomic_flag_test(&server->error_alert_received));
            EXPECT_TRUE(server->alert_sent);

            /* ensure s2n_send wrote a close notify */
            EXPECT_EQUAL(s2n_stuffer_data_available(&pair.client_in.data), S2N_ALERT_LENGTH);
            uint8_t alert[S2N_ALERT_LENGTH] = { level, code };
            uint8_t *ptr = s2n_stuffer_raw_read(&pair.client_in.data, S2N_ALERT_LENGTH);
            EXPECT_NOT_NULL(ptr);
            EXPECT_EQUAL(memcmp(alert, ptr, S2N_ALERT_LENGTH), 0);
        }

        /* Test sending fatal alert */
        {
            uint8_t io_buf[160] = { 0 };
            s2n_blocked_status blocked = S2N_NOT_BLOCKED;
            uint8_t record_type = TLS_ALERT;
            uint8_t level = S2N_TLS_ALERT_LEVEL_FATAL;

            uint8_t code = S2N_TLS_ALERT_HANDSHAKE_FAILURE;
            ssize_t size = 20;

            DEFER_CLEANUP(struct s2n_connection *server = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_connection *client = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_test_ktls_io_pair pair = { 0 },
                    s2n_ktls_io_pair_free);
            EXPECT_OK(s2n_test_init_ktls_conn_and_stuffer_io(server, client, &pair));
            pair.client_in.send_record_type = record_type;
            pair.client_in.send_record_type = record_type;

            /* ensure server state before alert */
            EXPECT_FALSE(s2n_atomic_flag_test(&server->read_closed));
            EXPECT_FALSE(s2n_atomic_flag_test(&server->write_closed));
            EXPECT_FALSE(s2n_atomic_flag_test(&server->close_notify_received));
            EXPECT_FALSE(s2n_atomic_flag_test(&server->error_alert_received));
            EXPECT_FALSE(server->alert_sent);

            /* write alert */
            EXPECT_SUCCESS(s2n_queue_reader_handshake_failure_alert(server));
            ssize_t send_ret = s2n_shutdown_send(server, &blocked);
            EXPECT_EQUAL(send_ret, 0);

            /* ensure connection state after alert.
             *
             * Writing alerts, doesnt set connection state; it only transmits the
             * alert. The Application is responsible for calling shutdown when it
             * receives a S2N_ERR_CLOSED error from IO. */
            EXPECT_FALSE(s2n_atomic_flag_test(&server->read_closed));
            EXPECT_FALSE(s2n_atomic_flag_test(&server->write_closed));
            EXPECT_FALSE(s2n_atomic_flag_test(&server->close_notify_received));
            EXPECT_FALSE(s2n_atomic_flag_test(&server->error_alert_received));
            EXPECT_TRUE(server->alert_sent);

            /* ensure s2n_send wrote a fatal alert */
            uint8_t alert[S2N_ALERT_LENGTH] = { level, code };
            EXPECT_EQUAL(s2n_stuffer_data_available(&pair.client_in.data), S2N_ALERT_LENGTH);
            uint8_t *ptr = s2n_stuffer_raw_read(&pair.client_in.data, S2N_ALERT_LENGTH);
            EXPECT_NOT_NULL(ptr);
            EXPECT_EQUAL(memcmp(alert, ptr, S2N_ALERT_LENGTH), 0);
        }
    }

    /* Test edge cases */
    {
        /* Test sending handshake record */
        {
            s2n_blocked_status blocked = S2N_NOT_BLOCKED;
            uint8_t record_type = TLS_HANDSHAKE;
            ssize_t result = -1;
            struct iovec iov;
            iov.iov_base = (void *) (uintptr_t) test_data;
            iov.iov_len = 5;

            DEFER_CLEANUP(struct s2n_connection *server = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_connection *client = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_test_ktls_io_pair pair = { 0 },
                    s2n_ktls_io_pair_free);
            EXPECT_OK(s2n_test_init_ktls_conn_and_stuffer_io(server, client, &pair));
            pair.client_in.send_record_type = record_type;

            EXPECT_ERROR_WITH_ERRNO(s2n_ktls_send(server, &iov, 1, record_type, &blocked, &result), S2N_ERR_UNIMPLEMENTED);
        }

        /* Test receiving handshake record. renegotiate and other handshake msg */
        {
            uint8_t io_buf[160] = { 0 };
            s2n_blocked_status blocked = S2N_NOT_BLOCKED;
            ssize_t size = 2;
            uint8_t record_type = TLS_HANDSHAKE;

            DEFER_CLEANUP(struct s2n_connection *server = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_connection *client = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            DEFER_CLEANUP(struct s2n_test_ktls_io_pair pair = { 0 },
                    s2n_ktls_io_pair_free);
            EXPECT_OK(s2n_test_init_ktls_conn_and_stuffer_io(server, client, &pair));

            uint8_t handshake[4] = { 0 };

            /* write "handshake" record. The type/len is written to ancillary stuffer */
            handshake[0] = TLS_HELLO_REQUEST;
            EXPECT_SUCCESS(s2n_stuffer_write_bytes(&pair.client_in.data, handshake, size));
            EXPECT_SUCCESS(s2n_stuffer_write_uint8(&pair.client_in.ancillary, record_type));
            EXPECT_SUCCESS(s2n_stuffer_write_uint8(&pair.client_in.ancillary, size));

            ssize_t result = -1;
            EXPECT_FAILURE_WITH_ERRNO(s2n_recv(client, io_buf, size, &blocked), S2N_ERR_IO_BLOCKED);
            EXPECT_EQUAL(blocked, S2N_BLOCKED_ON_READ);

            /* write "handshake" record. The type/len is written to ancillary stuffer */
            handshake[0] = SERVER_NEW_SESSION_TICKET;
            EXPECT_SUCCESS(s2n_stuffer_write_bytes(&pair.client_in.data, handshake, size));
            EXPECT_SUCCESS(s2n_stuffer_write_uint8(&pair.client_in.ancillary, record_type));
            EXPECT_SUCCESS(s2n_stuffer_write_uint8(&pair.client_in.ancillary, size));
            EXPECT_FAILURE_WITH_ERRNO(s2n_recv(client, io_buf, size, &blocked), S2N_ERR_BAD_MESSAGE);
        }
    }

    /* D- clean up testing */
    /* - test other cases */
    /* - error handling io */
    /* - detect handshake msg type */

    /* TODO */
    /* recv 1, send 1 alert fragment */

    /* D- send multiple times, recv once */
    /* N- can we reuse the exiting stuffer IO */
    /* D- type, len, data */
    /* D- send, recv, recv (blocked) */
    /* D- separate stuffer for read and write */

    /* handshake msg: renegotiate_request_cb -> ignore or kill conn? */

    /* rm  recv: treat warning as fatal unless we are treating it as non-fatal */

    END_TEST();
}
