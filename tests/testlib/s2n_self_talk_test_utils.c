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

#include "testlib/s2n_testlib.h"

S2N_RESULT noop_cb(struct s2n_connection *conn)
{
    return S2N_RESULT_OK;
}

const struct self_talk_inet_socket_callbacks noop_inet_cb = {
    .s_post_handshake_cb = noop_cb,
    .c_post_handshake_cb = noop_cb,
};
