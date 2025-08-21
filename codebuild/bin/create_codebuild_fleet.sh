#!/bin/bash

# Platform constants
PLATFORM_X86_64="x86_64"
PLATFORM_ARM64="aarch64"

# Region to CodeBuild Organization ARN mapping
# From a static table at
# https://docs.aws.amazon.com/codebuild/latest/userguide/fleets.reserved-capacity-fleets.html
declare -A REGION_TO_ORG_ARN=(
    ["us-east-1"]="arn:aws:organizations::851725618577:organization/o-c6wcu152r1"
    ["us-east-2"]="arn:aws:organizations::992382780434:organization/o-seufr2suvq"
    ["us-west-2"]="arn:aws:organizations::381491982620:organization/o-0412o99a4r"
    ["ap-northeast-1"]="arn:aws:organizations::891376993293:organization/o-b6k3sjqavm"
    ["ap-south-1"]="arn:aws:organizations::891376924779:organization/o-krtah1lkeg"
    ["ap-southeast-1"]="arn:aws:organizations::654654522137:organization/o-mcn8uvc3tp"
    ["ap-southeast-2"]="arn:aws:organizations::767398067170:organization/o-6crt0f6bu4"
    ["eu-central-1"]="arn:aws:organizations::590183817084:organization/o-lb2lne3te6"
    ["eu-west-1"]="arn:aws:organizations::891376938588:organization/o-ullrrg5qf0"
    ["sa-east-1"]="arn:aws:organizations::533267309133:organization/o-db63c45ozw"
)

# Function to display usage information
usage() {
    cat << EOF
Usage: $(basename "$0") [options]

Create a CodeBuild fleet using a custom AMI.

Options:
  --ami-id ID               The ID of the custom AMI to use
  --platform TYPE           The platform architecture (x86_64 or aarch64)
  --fleet-name NAME         The name to use for the CodeBuild fleet
  --service-role-arn ARN    The ARN of the service role for CodeBuild
  --region REGION           The AWS region to create the fleet in
  --help                    Display this help message and exit

Example:
  $(basename "$0") --ami-id ami-12345abcde --platform x86_64 --fleet-name s2n-codebuild-fleet --service-role-arn arn:aws:iam::123456789012:role/CodeBuildServiceRole --region us-east-1
EOF
}

# Function to ensure IAM role has the necessary permissions
ensure_iam_permissions() {
    local role_arn="$1"
    local region="$2"

    # Extract the role name from the ARN
    local role_name=$(echo "$role_arn" | cut -d'/' -f2)

    echo "Checking permissions for IAM role: $role_name"

    # Check if the role already has the ec2:DescribeImages permission
    if aws iam get-role-policy --role-name "$role_name" --policy-name codebuild-ec2-permissions --region "$region" 2>/dev/null | grep -q "ec2:DescribeImages"; then
        echo "IAM role already has ec2:DescribeImages permission."
        return 0
    fi

    # Check if the policy exists but doesn't have the permission
    local policy_exists=0
    local policy_document=''

    if aws iam get-role-policy --role-name "$role_name" --policy-name codebuild-ec2-permissions --region "$region" &>/dev/null; then
        policy_exists=1
        echo "Existing codebuild-ec2-permissions policy found. Updating..."

        # Get current policy
        local current_policy=$(aws iam get-role-policy --role-name "$role_name" --policy-name codebuild-ec2-permissions --region "$region" --query 'PolicyDocument' --output json)

        # Create a temporary file for the current policy
        local temp_policy_file=$(mktemp)
        echo "$current_policy" > "$temp_policy_file"

        # Check if the policy has a Statement array or single Statement
        if grep -q "\"Statement\":\\s*\\[" "$temp_policy_file"; then
            # Multi-statement policy - add our statement to the array
            policy_document=$(jq '.Statement += [{"Effect": "Allow", "Action": "ec2:DescribeImages", "Resource": "*"}]' "$temp_policy_file")
        else
            # Single statement policy - convert to array with both statements
            policy_document=$(jq '. += {"Statement": [.Statement, {"Effect": "Allow", "Action": "ec2:DescribeImages", "Resource": "*"}]}' "$temp_policy_file")
        fi

        # Clean up
        rm "$temp_policy_file"
    else
        # Create a new policy document with just the required permission
        policy_document='{
            "Version": "2012-10-17",
            "Statement": [
                {
                    "Effect": "Allow",
                    "Action": "ec2:DescribeImages",
                    "Resource": "*"
                }
            ]
        }'
    fi

    echo "Adding ec2:DescribeImages permission to IAM role..."

    # Add the policy to the role
    aws iam put-role-policy \
        --role-name "$role_name" \
        --policy-name codebuild-ec2-permissions \
        --policy-document "$policy_document" \
        --region "$region"

    if [ $? -eq 0 ]; then
        echo "Successfully added ec2:DescribeImages permission to IAM role."
        return 0
    else
        echo "Error: Failed to add ec2:DescribeImages permission to IAM role."
        return 1
    fi
}

# Function to create a CodeBuild fleet
create_codebuild_fleet() {
    local ami_id="$1"
    local platform="$2"
    local fleet_name="$3"
    local service_role_arn="$4"
    local region="$5"
    
    # Validate region
    if [ -z "${REGION_TO_ORG_ARN[$region]}" ]; then
        echo "Error: Region $region is not supported or not found in the mapping."
        return 1
    fi
    
    # Get organization ARN for the region
    local org_arn="${REGION_TO_ORG_ARN[$region]}"
    
    # Determine instance type based on platform
    local instance_type
    if [ "$platform" = "$PLATFORM_X86_64" ]; then
        instance_type="c7a.xlarge"  # Intel/AMD x86_64 instance
    elif [ "$platform" = "$PLATFORM_ARM64" ]; then
        instance_type="c7g.xlarge"  # AWS Graviton ARM instance
    else
        echo "Error: Invalid platform. Must be x86_64 or aarch64."
        return 1
    fi
    
    echo "Creating CodeBuild fleet in region $region with organization ARN: $org_arn"
    
    # Step 1: Grant CodeBuild access to the AMI
    echo "Granting CodeBuild access to AMI $ami_id..."
    aws ec2 modify-image-attribute \
        --image-id "$ami_id" \
        --launch-permission "Add=[{OrganizationArn=$org_arn}]" \
        --region "$region"
        
    if [ $? -ne 0 ]; then
        echo "Error: Failed to grant CodeBuild access to AMI."
        return 1
    fi
    
    # Step 2: Create the CodeBuild Fleet
    echo "Creating CodeBuild fleet $fleet_name-$platform using instance type $instance_type..."
    aws codebuild create-fleet \
        --name "$fleet_name-$platform" \
        --fleet-service-role "$service_role_arn" \
        --base-capacity 2 \
        --environment-type LINUX_EC2 \
        --compute-type "CUSTOM_INSTANCE_TYPE" \
        --compute-configuration "instanceType=$instance_type" \
        --image-id "$ami_id" \
        --region "$region"
        
    if [ $? -eq 0 ]; then
        echo "Successfully created CodeBuild fleet: $fleet_name-$platform in region $region"
    else
        echo "Error: Failed to create CodeBuild fleet."
        return 1
    fi
}

# Parse command-line arguments
parse_args() {
    # Default values
    AMI_ID=""
    PLATFORM="x86_64"
    FLEET_NAME=""
    SERVICE_ROLE_ARN=""
    REGION="us-west-2"

    # Loop through arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --ami-id)
                if [[ -n "$2" && "$2" != --* ]]; then
                    AMI_ID="$2"
                    shift 2
                else
                    echo "Error: --ami-id requires an argument"
                    usage
                    exit 1
                fi
                ;;
            --platform)
                if [[ -n "$2" && "$2" != --* ]]; then
                    PLATFORM="$2"
                    shift 2
                else
                    echo "Error: --platform requires an argument"
                    usage
                    exit 1
                fi
                ;;
            --fleet-name)
                if [[ -n "$2" && "$2" != --* ]]; then
                    FLEET_NAME="$2"
                    shift 2
                else
                    echo "Error: --fleet-name requires an argument"
                    usage
                    exit 1
                fi
                ;;
            --service-role-arn)
                if [[ -n "$2" && "$2" != --* ]]; then
                    SERVICE_ROLE_ARN="$2"
                    shift 2
                else
                    echo "Error: --service-role-arn requires an argument"
                    usage
                    exit 1
                fi
                ;;
            --region)
                if [[ -n "$2" && "$2" != --* ]]; then
                    REGION="$2"
                    shift 2
                else
                    echo "Error: --region requires an argument"
                    usage
                    exit 1
                fi
                ;;
            --help)
                usage
                exit 0
                ;;
            *)
                echo "Error: Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done

    # Validate required parameters
    if [[ -z "$AMI_ID" ]]; then
        echo "Error: --ami-id is required"
        usage
        exit 1
    fi
    
    if [[ -z "$PLATFORM" ]]; then
        echo "Error: --platform is required"
        usage
        exit 1
    elif [[ "$PLATFORM" != "$PLATFORM_X86_64" && "$PLATFORM" != "$PLATFORM_ARM64" ]]; then
        echo "Error: --platform must be either $PLATFORM_X86_64 or $PLATFORM_ARM64"
        usage
        exit 1
    fi
    
    if [[ -z "$FLEET_NAME" ]]; then
        echo "Error: --fleet-name is required"
        usage
        exit 1
    fi
    
    if [[ -z "$SERVICE_ROLE_ARN" ]]; then
        echo "Error: --service-role-arn is required"
        usage
        exit 1
    fi
    
    if [[ -z "$REGION" ]]; then
        echo "Error: --region is required"
        usage
        exit 1
    fi
}

# Main function
parse_args "$@"

# Ensure IAM role has necessary permissions
echo "Ensuring IAM role has necessary permissions..."
ensure_iam_permissions "$SERVICE_ROLE_ARN" "$REGION" || exit 1

# Create the CodeBuild fleet
create_codebuild_fleet "$AMI_ID" "$PLATFORM" "$FLEET_NAME" "$SERVICE_ROLE_ARN" "$REGION"
return $?
