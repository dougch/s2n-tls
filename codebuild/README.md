# Docker Image Structure
The codebuild specifications are run on a custom docker images that have the test dependencies installed. The docker image structure is described below.

### libcrypto
Various libcryptos are installed to `/usr/local/$LIBCRYPTO` directories. For example
```
# non-exhaustive list
/usr/local/openssl-1.0.2/lib/libcrypto.a
/usr/local/openssl-1.0.2/lib/libcrypto.so
/usr/local/openssl-1.0.2/lib/libcrypto.so.1.0.0
/usr/local/openssl-1.0.2/lib/pkgconfig/libcrypto.pc
/usr/local/openssl-3.0/lib64/libcrypto.a
/usr/local/openssl-3.0/lib64/libcrypto.so.3
/usr/local/openssl-3.0/lib64/libcrypto.so
/usr/local/openssl-3.0/lib64/pkgconfig/libcrypto.pc
/usr/local/boringssl/lib/libcrypto.so
/usr/local/awslc/lib/libcrypto.a
/usr/local/awslc/lib/libcrypto.so
```

Packages installed from the `apt` package manager can generally be found in `/usr/lib`. For example, our 32 bit build uses the 32 bit `i386` libcrypto, and it's artifacts are located at
```
/usr/lib/i386-linux-gnu/libcrypto.a
/usr/lib/i386-linux-gnu/libcrypto.so.3
/usr/lib/i386-linux-gnu/libcrypto.so
/usr/lib/i386-linux-gnu/pkgconfig/libcrypto.pc
```

When the docker image is available locally, the structure can be easily examined by attaching an interactive terminal to the container with the following command
```
docker run --entrypoint /bin/bash -it --privileged <image id>
```

Then the `find` command can be used to look at the various artifacts that are available.
```
sudo find / -name libcrypto* # list all libcrypto artifacts
```
or
```
sudo find / -name clang* # find all clang binaries
```

# CodeBuild Fleets using EC2

This section describes the process for creating custom EC2-based CodeBuild fleets for s2n-tls builds.
Our main use-case for CodeBuild Ec2 is our kTLS tests, which need to load a kernel module.

## Creating a Custom AMI

### Prepare an EC2 Instance
- Using the Ec2 Console or the AWS cli, launch a new instance using an AMI from a trusted provider (Amazon, Canonical, Red Hat, etc.)
- Select an appropriate instance type and platform
- Launch the instance

### Update and Install Dependencies
- Update the system packages to the latest versions
- Install git

### Install and Configure Nix
- Install Nix
- Configure Nix to work for the root user (required for CodeBuild)
- Verify the Nix installation and reduce the store size by running `sudo nix store gc`

### Create an AMI Snapshot
- In the EC2 Console, or using the AWS cli, create a snapshot from your configured instance
- Provide a descriptive name for the AMI
- Wait for the AMI creation process to complete
- Note the AMI ID for future reference

## Create a CodeBuild Fleet

Using an AMI/snapshot in your own account as input, these steps walk you through creating a CodeBuild fleet.

### Grant CodeBuild Access to the AMI
- Find the CodeBuild service account ARN for your region
- Modify AMI permissions to allow the CodeBuild service account to access the ami

### Create the CodeBuild Fleet
- Make sure you have Admin/full rights to CodeBuild in your account.
- Navigate to the CodeBuild console, or use the AWS cli
- Create a new build fleet using your custom AMI
- Configure fleet settings (instance types, capacity, VPC settings, etc.)

Once completed, your custom EC2-based CodeBuild fleet will be available for running s2n-tls builds with Nix support.
