#!/usr/bin/env bash
#
#

set -eu
BUILDS=(
    "AddressSanitizer"
    "S2nIntegrationV2SmallBatch"
    "Valgrind"
    "s2nFuzzBatch"
    "s2nGeneralBatch"
    "kTLS"
    "Openssl3fipsWIP"
)
buildspec() {
	aws codebuild batch-get-projects --name $1 | jq '.projects[].source.buildspec'
}

for each in ${BUILDS[@]}; do
	echo -e "$each\t"
	buildspec $each
done;
