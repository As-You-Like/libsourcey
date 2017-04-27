# CMake build commands

## Doxygen

git clone https://github.com/doxygen/doxygen.git
git checkout tags/Release_1_8_12
cd doxygen
mkdir build
cd build
cmake -G "Unix Makefiles" ..
make
sudo make install

doxygen -g
INPUT=/home/kam/sourcey/libsourcey/src
OUTPUT_DIRECTORY=/home/kam/sourcey/libsourcey/build/doxygen
PROJECT_NAME="LibSourcey"
RECURSIVE=YES
GENERATE_XML=NO
GENERATE_HTML=NO
GENERATE_LATEX=NO
EXCLUDE_PATTERNS = */anionu* */test* */apps* */samples*

npm install moxygen
./node_modules/moxygen/index.js --verbose ./doc/xml > API.md
node ./moxygen/bin/moxygen.js --verbose ./biuld/doxygen/xml
node ./moxygen/bin/moxygen.js --verbose --groups --output=./doc ./build/doxygen/xml
node ./moxygen/bin/moxygen.js --verbose --groups --output ./doc/api.md ./build/doxygen/xml

npm install gitbook-cli
./node_modules/gitbook-cli/bin/gitbook.js init

https://github.com/contao/docs/blob/master/cookbook/book.json

## Default debug build
cmake .. -DCMAKE_BUILD_TYPE=DEBUG -DBUILD_SHARED_LIBS=ON \
         -DBUILD_MODULES=ON -DBUILD_APPLICATIONS=ON \
         -DBUILD_SAMPLES=ON -DBUILD_TESTS=ON \
         -DWITH_FFMPEG=ON

## Default debug build (with WebRTC)
cmake .. -DCMAKE_BUILD_TYPE=DEBUG -DBUILD_SHARED_LIBS=ON \
         -DBUILD_MODULES=ON -DBUILD_APPLICATIONS=ON \
         -DBUILD_SAMPLES=ON -DBUILD_TESTS=ON \
         -DWITH_FFMPEG=ON -DWITH_WEBRTC=ON \
         -DWEBRTC_ROOT_DIR=/home/kam/sourcey/webrtcbuilds/out/src

## All modules (selective)
cmake .. -DCMAKE_BUILD_TYPE=DEBUG -DBUILD_SHARED_LIBS=OFF \
         -DBUILD_APPLICATIONS=OFF -DBUILD_MODULES=OFF \
         -DBUILD_SAMPLES=ON -DBUILD_TESTS=ON \
         -DWITH_FFMPEG=ON -DWITH_OPENCV=OFF \
         -DBUILD_MODULE_archo=ON \
         -DBUILD_MODULE_base=ON -DBUILD_MODULE_crypto=ON \
         -DBUILD_MODULE_http=ON -DBUILD_MODULE_json=ON \
         -DBUILD_MODULE_av=ON -DBUILD_MODULE_net=ON \
         -DBUILD_MODULE_pacm=ON -DBUILD_MODULE_pluga=ON \
         -DBUILD_MODULE_sked=ON -DBUILD_MODULE_socketio=ON \
         -DBUILD_MODULE_stun=ON -DBUILD_MODULE_symple=ON \
         -DBUILD_MODULE_turn=ON -DBUILD_MODULE_util=ON \
         -DBUILD_MODULE_uv=ON -DBUILD_MODULE_webrtc=ON

## Minimum build (uv, base)
cmake .. -DCMAKE_BUILD_TYPE=DEBUG -DBUILD_MODULES=OFF \
         -DBUILD_MODULE_base=ON -DBUILD_MODULE_uv=ON

cmake .. -DCMAKE_BUILD_TYPE=DEBUG -DBUILD_MODULES=OFF \
         -DBUILD_MODULE_base=ON -DBUILD_MODULE_uv=ON \
         -DBUILD_SHARED_LIBS=ON

## Media build
cmake .. -DCMAKE_BUILD_TYPE=DEBUG -DBUILD_SHARED_LIBS=OFF \
         -DBUILD_APPLICATIONS=OFF -DBUILD_MODULES=OFF \
         -DBUILD_SAMPLES=ON -DBUILD_TESTS=ON \
         -DWITH_FFMPEG=ON -DWITH_OPENCV=OFF \
         -DBUILD_MODULE_av=ON -DBUILD_MODULE_base=ON \
         -DBUILD_MODULE_uv=ON

## Symple build (no media)
cmake .. -DCMAKE_BUILD_TYPE=DEBUG -DBUILD_SHARED_LIBS=OFF \
         -DBUILD_APPLICATIONS=ON -DBUILD_MODULES=OFF \
         -DBUILD_SAMPLES=ON -DBUILD_TESTS=ON \
         -DWITH_FFMPEG=OFF -DWITH_OPENCV=OFF \
         -DBUILD_MODULE_archo=ON -DBUILD_MODULE_base=ON \
         -DBUILD_MODULE_crypto=ON -DBUILD_MODULE_http=ON \
         -DBUILD_MODULE_json=ON -DBUILD_MODULE_net=ON \
         -DBUILD_MODULE_socketio=ON -DBUILD_MODULE_symple=ON \
         -DBUILD_MODULE_util=ON -DBUILD_MODULE_uv=ON

## Symple build (with media)
cmake .. -DCMAKE_BUILD_TYPE=DEBUG -DBUILD_SHARED_LIBS=OFF \
         -DBUILD_APPLICATIONS=ON -DBUILD_MODULES=OFF \
         -DBUILD_SAMPLES=ON -DBUILD_TESTS=ON \
         -DWITH_FFMPEG=ON -DWITH_OPENCV=OFF \
         -DBUILD_MODULE_archo=ON -DBUILD_MODULE_base=ON \
         -DBUILD_MODULE_crypto=ON -DBUILD_MODULE_http=ON \
         -DBUILD_MODULE_json=ON -DBUILD_MODULE_av=ON \
         -DBUILD_MODULE_net=ON -DBUILD_MODULE_socketio=ON \
         -DBUILD_MODULE_symple=ON -DBUILD_MODULE_util=ON \
         -DBUILD_MODULE_uv=ON

## HTTP build
cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DBUILD_SHARED_LIBS=OFF \
         -DBUILD_APPLICATIONS=ON -DBUILD_MODULES=OFF \
         -DBUILD_SAMPLES=ON -DBUILD_TESTS=ON \
         -DBUILD_MODULE_archo=ON -DBUILD_MODULE_base=ON \
         -DBUILD_MODULE_crypto=ON -DBUILD_MODULE_http=ON \
         -DBUILD_MODULE_json=ON -DBUILD_MODULE_net=ON \
         -DBUILD_MODULE_util=ON -DBUILD_MODULE_uv=ON

## STUN/TURN build
cmake .. -DCMAKE_BUILD_TYPE=DEBUG -DBUILD_SHARED_LIBS=OFF \
         -DBUILD_APPLICATIONS=ON -DBUILD_MODULES=OFF \
         -DBUILD_MODULE_base=ON -DBUILD_MODULE_crypto=ON \
         -DBUILD_MODULE_net=ON -DBUILD_MODULE_stun=ON \
         -DBUILD_MODULE_turn=ON -DBUILD_MODULE_util=OFF \
         -DBUILD_MODULE_uv=ON

## WebRTC (Win64, Static)
cmake -G "Visual Studio 15 Win64" .. -DCMAKE_BUILD_TYPE=DEBUG -DBUILD_SHARED_LIBS=OFF -DBUILD_MODULES=OFF -DBUILD_WITH_STATIC_CRT=ON -DBUILD_APPLICATIONS=ON -DBUILD_SAMPLES=ON -DBUILD_TESTS=ON -DWITH_WEBRTC=ON -DWITH_FFMPEG=ON -DBUILD_MODULE_base=ON -DBUILD_MODULE_crypto=ON -DBUILD_MODULE_http=ON -DBUILD_MODULE_json=ON -DBUILD_MODULE_av=ON -DBUILD_MODULE_net=ON -DBUILD_MODULE_socketio=ON -DBUILD_MODULE_symple=ON -DBUILD_MODULE_util=ON -DBUILD_MODULE_uv=ON -DBUILD_MODULE_stun=ON -DBUILD_MODULE_turn=ON -DBUILD_MODULE_webrtc=ON -DFFMPEG_ROOT_DIR=E:/dev/vendor/ffmpeg-3.2.2-win64-dev -DWEBRTC_ROOT_DIR=E:/dev/vendor/webrtcbuilds/out/webrtcbuilds-16123-52b6562-win-x64
