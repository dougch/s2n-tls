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

#include "s2n_test.h"
#include "testlib/s2n_testlib.h"
#include "tls/s2n_ktls.h"

S2N_RESULT s2n_ktls_send_msg_impl(int sock, struct msghdr *msg,
        struct iovec *msg_iov, size_t count, s2n_blocked_status *blocked,
        ssize_t *result);
S2N_RESULT s2n_ktls_recv_msg_impl(struct s2n_connection *conn, int sock, struct msghdr *msg,
        struct iovec *msg_iov, s2n_blocked_status *blocked, ssize_t *result);

S2N_RESULT s2n_ktls_send_control_msg(int sock, struct msghdr *msg,
        uint8_t record_type, s2n_blocked_status *blocked, ssize_t *result);
S2N_RESULT s2n_ktls_recv_control_msg(int sock, struct msghdr *msg,
        uint8_t *record_type, s2n_blocked_status *blocked, ssize_t *result);

#define TEST_MAX_DATA_LEN 20000
uint8_t TEST_SEND_RECORD_TYPE = 10;

S2N_RESULT helper_generate_test_data(struct s2n_blob *test_data)
{
    struct s2n_stuffer test_data_stuffer = { 0 };
    RESULT_GUARD_POSIX(s2n_stuffer_init(&test_data_stuffer, test_data));
    for (size_t i = 1; i < TEST_MAX_DATA_LEN; i++) {
        RESULT_GUARD_POSIX(s2n_stuffer_write_uint8(&test_data_stuffer, i));
    }
    return S2N_RESULT_OK;
}

int main(int argc, char **argv)
{
    BEGIN_TEST();

#if S2N_KTLS_SUPPORTED
    uint8_t test_data[TEST_MAX_DATA_LEN] = { 0 };
    struct s2n_blob test_data_blob = { 0 };
    EXPECT_SUCCESS(s2n_blob_init(&test_data_blob, test_data, sizeof(test_data)));
    EXPECT_OK(helper_generate_test_data(&test_data_blob));

    struct msghdr msg = { 0 };
    struct iovec msg_iov = { 0 };

    /* ctrl_msg send and recv data */
    for (size_t to_send = 1; to_send < TEST_MAX_DATA_LEN; to_send += 500) {
        /* Create a pipe */
        struct s2n_test_io_pair io_pair;
        EXPECT_SUCCESS(s2n_io_pair_init_non_blocking(&io_pair));
        s2n_blocked_status blocked = S2N_NOT_BLOCKED;

        {
            msg_iov.iov_base = (void *) (uintptr_t) test_data;
            msg_iov.iov_len = to_send;

            ssize_t sent_len = 0;
            EXPECT_OK(s2n_ktls_send_msg_impl(io_pair.client, &msg, &msg_iov, 1, &blocked, &sent_len));
            EXPECT_EQUAL(sent_len, to_send);
        }

        uint8_t recv_buffer[TEST_MAX_DATA_LEN] = { 0 };
        /* confirm test_data and recv_buffer dont match */
        EXPECT_NOT_EQUAL(memcmp(test_data, recv_buffer, to_send), 0);
        /* { */
        /*     msg_iov.iov_base = recv_buffer; */
        /*     msg_iov.iov_len = to_send; */

        /*     ssize_t recv_len = 0; */
        /*     EXPECT_OK(s2n_ktls_recv_msg_impl(io_pair.server, &msg, &msg_iov, &blocked, &recv_len)); */
        /*     EXPECT_EQUAL(recv_len, to_send); */
        /*     EXPECT_EQUAL(memcmp(test_data, recv_buffer, recv_len), 0); */
        /* } */
    }

    /* test blocked data and partial reads */
    {
        /* Create a pipe */
        struct s2n_test_io_pair io_pair;
        EXPECT_SUCCESS(s2n_io_pair_init_non_blocking(&io_pair));
        s2n_blocked_status blocked = S2N_NOT_BLOCKED;

        /* only read half the total data sent to simulate multiple reads */
        size_t to_send = 10;
        size_t to_recv = 5;

        uint8_t recv_buffer[TEST_MAX_DATA_LEN] = { 0 };
        /* confirm test_data and recv_buffer dont match */
        EXPECT_NOT_EQUAL(memcmp(test_data, recv_buffer, to_send), 0);

        /* calling recv when nothing has been sent blocks */
        /* { */
        /*     msg_iov.iov_base = recv_buffer; */
        /*     msg_iov.iov_len = to_recv; */

        /*     ssize_t recv_len = 0; */
        /*     EXPECT_ERROR_WITH_ERRNO(s2n_ktls_recv_msg_impl(io_pair.server, &msg, &msg_iov, &blocked, &recv_len), S2N_ERR_IO_BLOCKED); */
        /*     EXPECT_EQUAL(blocked, S2N_BLOCKED_ON_READ); */
        /* } */

        /* send data */
        {
            msg_iov.iov_base = (void *) (uintptr_t) test_data;
            msg_iov.iov_len = to_send;

            ssize_t sent_len = 0;
            EXPECT_OK(s2n_ktls_send_msg_impl(io_pair.client, &msg, &msg_iov, 1, &blocked, &sent_len));
            EXPECT_EQUAL(sent_len, to_send);
        }

        /* only read half the amount of data sent */
        /* { */
        /*     msg_iov.iov_base = recv_buffer; */
        /*     msg_iov.iov_len = to_recv; */

        /*     ssize_t recv_len = 0; */
        /*     EXPECT_OK(s2n_ktls_recv_msg_impl(io_pair.server, &msg, &msg_iov, &blocked, &recv_len)); */
        /*     EXPECT_EQUAL(recv_len, to_recv); */
        /*     EXPECT_EQUAL(memcmp(test_data, recv_buffer, to_recv), 0); */
        /* } */

        /* read the other half of data sent */
        /* { */
        /*     /1* offset the read buffer by the amount already read *1/ */
        /*     msg_iov.iov_base = recv_buffer + to_recv; */
        /*     msg_iov.iov_len = to_recv; */

        /*     ssize_t recv_len = 0; */
        /*     EXPECT_OK(s2n_ktls_recv_msg_impl(io_pair.server, &msg, &msg_iov, &blocked, &recv_len)); */
        /*     EXPECT_EQUAL(recv_len, to_recv); */
        /* } */

        /* confirm that all data was read and matches sent data of length `to_send` */
        /* EXPECT_EQUAL(memcmp(test_data, recv_buffer, to_send), 0); */
    }

    /* create and parse ancillary data */
    {
        int fd = 0;
        s2n_blocked_status blocked = S2N_NOT_BLOCKED;
        ssize_t result = 0;
        union {
            char buf[CMSG_SPACE(sizeof(TEST_SEND_RECORD_TYPE))];
            struct cmsghdr align;
        } control_msg;

        /* Init msghdr */
        struct msghdr s_msg = { 0 };
        s_msg.msg_control = control_msg.buf;
        s_msg.msg_controllen = sizeof(control_msg.buf);

        /* create the control_msg */
        EXPECT_OK(s2n_ktls_send_control_msg(fd, &s_msg, TEST_SEND_RECORD_TYPE, &blocked, &result));

        /* parse ancillary data */
        {
            /* modify control_msg for the recv side. cmsg_type is GET_RECORD_TYPE on the receiving socket */
            struct cmsghdr *hdr = CMSG_FIRSTHDR(&s_msg);
            hdr->cmsg_type = S2N_TLS_GET_RECORD_TYPE;

            /* assert that we can parse the same record_type */
            uint8_t recv_record_type = 0;
            EXPECT_OK(s2n_ktls_recv_control_msg(fd, &s_msg, &recv_record_type, &blocked, &result));
            EXPECT_EQUAL(recv_record_type, TEST_SEND_RECORD_TYPE);

            /* record_type should default to TLS_APPLICATION_DATA */
            hdr->cmsg_type = 0;
            hdr->cmsg_level = 0;
            recv_record_type = 0;
            EXPECT_ERROR_WITH_ERRNO(s2n_ktls_recv_control_msg(fd, &s_msg, &recv_record_type, &blocked, &result), S2N_ERR_IO);
        }
    }

    /* create and parse ancillary data */
    {
        int fd = 0;
        s2n_blocked_status blocked = S2N_NOT_BLOCKED;
        ssize_t result = 0;
        union {
            /* Space large enough to hold 2 record_type */
            char buf[CMSG_SPACE(sizeof(TEST_SEND_RECORD_TYPE)) * 2];
            struct cmsghdr align;
        } control_msg;

        /* Init msghdr */
        struct msghdr s_msg = { 0 };
        memset(&control_msg.buf, 0, sizeof(control_msg.buf));
        s_msg.msg_control = control_msg.buf;
        s_msg.msg_controllen = sizeof(control_msg.buf);

        /* create the control_msg */
        EXPECT_OK(s2n_ktls_send_control_msg(fd, &s_msg, TEST_SEND_RECORD_TYPE, &blocked, &result));

        /* parse control_msg */
        /* modify control_msg for the recv side */
        struct cmsghdr *hdr = CMSG_FIRSTHDR(&s_msg);
        hdr->cmsg_type = S2N_TLS_GET_RECORD_TYPE;
        {
            uint8_t recv_record_type = 0;
            /* assert that we can parse the same record_type */
            {
                EXPECT_OK(s2n_ktls_recv_control_msg(fd, &s_msg, &recv_record_type, &blocked, &result));
                EXPECT_EQUAL(recv_record_type, TEST_SEND_RECORD_TYPE);
            }

            /* modify first hdr so that level doesnt match S2N_SOL_TLS */
            {
                hdr->cmsg_level = 0;

                recv_record_type = 0;
                EXPECT_ERROR_WITH_ERRNO(s2n_ktls_recv_control_msg(fd, &s_msg, &recv_record_type, &blocked, &result), S2N_ERR_IO);
            }

            /* should search all possible cmsg for record_type
             *
             * add a second (CMSG_NXTHDR) header with the record_type
             */
            {
                /* confirm first header doesnt match record_type */
                EXPECT_ERROR_WITH_ERRNO(s2n_ktls_recv_control_msg(fd, &s_msg, &recv_record_type, &blocked, &result), S2N_ERR_IO);

                /* add second cmsg with record_type */
                struct cmsghdr *nxt = CMSG_NXTHDR(&s_msg, hdr);
                EXPECT_NOT_NULL(nxt);
                nxt->cmsg_level = S2N_SOL_TLS;
                nxt->cmsg_type = S2N_TLS_GET_RECORD_TYPE;
                nxt->cmsg_len = CMSG_LEN(sizeof(uint8_t));
                POSIX_CHECKED_MEMCPY(CMSG_DATA(nxt), &TEST_SEND_RECORD_TYPE, sizeof(TEST_SEND_RECORD_TYPE));

                recv_record_type = 0;
                EXPECT_OK(s2n_ktls_recv_control_msg(fd, &s_msg, &recv_record_type, &blocked, &result));
                EXPECT_EQUAL(recv_record_type, TEST_SEND_RECORD_TYPE);
            }
        }
    }
#else
    {
        char buf[sizeof(uint8_t)];
        int fd = 0;
        s2n_blocked_status blocked = S2N_NOT_BLOCKED;
        ssize_t result = 0;
        uint8_t record_type = 0;

        /* Init msghdr */
        struct msghdr msg = { 0 };
        msg.msg_control = buf;
        msg.msg_controllen = sizeof(record_type);

        /* create the control_msg */
        EXPECT_ERROR_WITH_ERRNO(s2n_ktls_send_control_msg(fd, &msg, record_type, &blocked, &result), S2N_ERR_KTLS_UNSUPPORTED_PLATFORM);

        EXPECT_ERROR_WITH_ERRNO(s2n_ktls_recv_control_msg(fd, &msg, &record_type, &blocked, &result), S2N_ERR_KTLS_UNSUPPORTED_PLATFORM);
    }
#endif

    END_TEST();
}
