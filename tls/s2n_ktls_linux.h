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

/*
 * Linux doesn't expose kTLS headers in its uapi. Its possible to get these headers
 * via glibc but support can vary depending on the version of glibc on the host.
 * Instead we define linux specific values inline.
 *
 * - https://elixir.bootlin.com/linux/v6.3.8/A/ident/TCP_ULP
 * - https://elixir.bootlin.com/linux/v6.3.8/A/ident/SOL_TCP
 */

/* socket definitions */
#define S2N_SOL_TLS 282

/* cmsg */
#define S2N_TLS_SET_RECORD_TYPE 1
#define S2N_TLS_GET_RECORD_TYPE 2
