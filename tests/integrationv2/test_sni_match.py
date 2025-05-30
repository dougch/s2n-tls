# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
import pytest

from configuration import available_ports, MULTI_CERT_TEST_CASES
from common import ProviderOptions, Protocols
from fixtures import managed_process  # noqa: F401
from providers import Provider, S2N, OpenSSL
from utils import (
    invalid_test_parameters,
    get_parameter_name,
    get_expected_s2n_version,
    to_bytes,
)


def filter_cipher_list(*args, **kwargs):
    """
    The framework normally filters out ciphers that are not supported by the chosen
    protocol. That doesn't happen in this test because of the unique way ciphers are
    grouped for the multi certificate tests.

    This function handles that unique grouping.
    """
    protocol = kwargs.get("protocol")
    cert_test_case = kwargs.get("cert_test_case")

    lowest_protocol_cipher = min(
        cert_test_case.client_ciphers, key=lambda x: x.min_version
    )
    if protocol < lowest_protocol_cipher.min_version:
        return True

    return invalid_test_parameters(*args, **kwargs)


@pytest.mark.uncollect_if(func=filter_cipher_list)
@pytest.mark.parametrize("provider", [OpenSSL], ids=get_parameter_name)
@pytest.mark.parametrize("other_provider", [S2N], ids=get_parameter_name)
@pytest.mark.parametrize(
    "protocol", [Protocols.TLS13, Protocols.TLS12], ids=get_parameter_name
)
@pytest.mark.parametrize("cert_test_case", MULTI_CERT_TEST_CASES)
def test_sni_match(managed_process, provider, other_provider, protocol, cert_test_case):  # noqa: F811
    port = next(available_ports)

    client_options = ProviderOptions(
        mode=Provider.ClientMode,
        port=port,
        insecure=False,
        verify_hostname=True,
        server_name=cert_test_case.client_sni,
        cipher=cert_test_case.client_ciphers,
        protocol=protocol,
    )

    server_options = ProviderOptions(
        mode=Provider.ServerMode, port=port, extra_flags=[], protocol=protocol
    )

    # Setup the certificate chain for S2ND based on the multicert test case
    cert_key_list = [(cert[0], cert[1]) for cert in cert_test_case.server_certs]
    for cert_key_path in cert_key_list:
        server_options.extra_flags.extend(["--cert", cert_key_path[0]])
        server_options.extra_flags.extend(["--key", cert_key_path[1]])

    server = managed_process(S2N, server_options, timeout=5)
    client = managed_process(provider, client_options, timeout=5)

    for results in client.get_results():
        results.assert_success()

    expected_version = get_expected_s2n_version(protocol, provider)

    for results in server.get_results():
        results.assert_success()
        assert (
            to_bytes("Actual protocol version: {}".format(expected_version))
            in results.stdout
        )
        if cert_test_case.client_sni is not None:
            assert (
                to_bytes("Server name: {}".format(cert_test_case.client_sni))
                in results.stdout
            )
