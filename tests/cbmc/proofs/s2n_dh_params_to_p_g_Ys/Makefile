#
#
# Licensed under the Apache License, Version 2.0 (the "License"). You may not use
# this file except in compliance with the License. A copy of the License is
# located at
#
#     http://aws.amazon.com/apache2.0/
#
# or in the "license" file accompanying this file. This file is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied. See the License for the specific language governing permissions and
# limitations under the License.

# Expected runtime is 5 minutes.

CHECKFLAGS +=

PROOF_UID = s2n_dh_params_to_p_g_Ys
HARNESS_ENTRY = $(PROOF_UID)_harness
HARNESS_FILE = $(HARNESS_ENTRY).c

PROOF_SOURCES += $(PROOFDIR)/$(HARNESS_FILE)
PROOF_SOURCES += $(OPENSSL_SOURCE)/dh_override.c
PROOF_SOURCES += $(OPENSSL_SOURCE)/bn_override.c
PROOF_SOURCES += $(PROOF_SOURCE)/cbmc_utils.c
PROOF_SOURCES += $(PROOF_SOURCE)/make_common_datastructures.c
PROOF_SOURCES += $(PROOF_STUB)/mlock.c
PROOF_SOURCES += $(PROOF_STUB)/munlock.c
PROOF_SOURCES += $(PROOF_STUB)/sysconf.c
PROOF_SOURCES += $(PROOF_STUB)/s2n_calculate_stacktrace.c

PROJECT_SOURCES += $(SRCDIR)/crypto/s2n_dhe.c
PROJECT_SOURCES += $(SRCDIR)/stuffer/s2n_stuffer.c
PROJECT_SOURCES += $(SRCDIR)/stuffer/s2n_stuffer_network_order.c
PROJECT_SOURCES += $(SRCDIR)/utils/s2n_blob.c
PROJECT_SOURCES += $(SRCDIR)/utils/s2n_mem.c
PROJECT_SOURCES += $(SRCDIR)/utils/s2n_safety.c

REMOVE_FUNCTION_BODY += __CPROVER_file_local_s2n_mem_c_s2n_mem_cleanup_impl
REMOVE_FUNCTION_BODY += s2n_free
REMOVE_FUNCTION_BODY += s2n_realloc
REMOVE_FUNCTION_BODY += s2n_stuffer_wipe

UNWINDSET += s2n_stuffer_write_network_order.10:9

include ../Makefile.common
