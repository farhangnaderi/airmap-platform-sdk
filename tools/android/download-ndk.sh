#!/bin/bash
set -ex

version=$1
outputDir=$2

case "${version}" in
    android-ndk-r16b)
        url=https://dl.google.com/android/repository/android-ndk-r16b-linux-x86_64.zip
        file=android-ndk-r16b-linux-x86_64.zip
        sha1=42aa43aae89a50d1c66c3f9fdecd676936da6128
        ;;
    android-ndk-r17c)
        url=https://dl.google.com/android/repository/android-ndk-r17c-linux-x86_64.zip
        file=android-ndk-r17c-linux-x86_64.zip
        sha1=12cacc70c3fd2f40574015631c00f41fb8a39048
        ;;
    android-ndk-r21d)
        url=https://dl.google.com/android/repository/android-ndk-r21d-linux-x86_64.zip
        file=android-ndk-r21d-linux-x86_64.zip
        sha1=bcf4023eb8cb6976a4c7cff0a8a8f145f162bf4d
        ;;
    android-ndk-r25b)
        url=https://dl.google.com/android/repository/android-ndk-r25b-linux.zip
        file=android-ndk-r25b-linux.zip
        sha1=e27dcb9c8bcaa77b78ff68c3f23abcf6867959eb
        ;;
    *)
        echo "unknown NDK version ${version}"
        exit 1
        ;;
esac

output=$outputDir/${file}

if ! [ -e ${output} ]; then
    curl --output ${output} ${url} 2> /dev/null
fi

echo "${sha1} ${output}" | sha1sum -c - > /dev/null

dir=$(unzip -Z -1 ${output} | head -1)
unzip -qq -o ${output} -d $outputDir
rm ${output}
echo ${outputDir}/${dir}