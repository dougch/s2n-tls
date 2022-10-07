#!/bin/bash
set -eu

AWS_S3_BUCKET=TODO

build_cmake() {
         wget https://github.com/Kitware/CMake/releases/download/v3.20.0/cmake-3.20.0.tar.gz -O /tmp/cmake.tgz
         tar -zvxf /tmp/cmake.tgz -C /tmp
         cd /tmp/cmake-3.20.0
       ./bootstrap && maske -j$(nproc) install
}

upload_to_s3(){
    echo "Uploading $1 to s3..."
    aws s3 cp $1 $AWS_S3_BUCKET/ 
    echo "Done"
}


build_cmake
upload_to_s3
