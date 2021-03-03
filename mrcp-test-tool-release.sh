#./bootstrap
#./configure
make
make install
rm -rf release mrcp-test-tool.zip
mkdir release
cd release
mkdir bin conf conf/client-profiles data lib log var
#bin
cp /usr/local/unimrcp/bin/asrclient bin/
#conf
cp /usr/local/unimrcp/conf/client-profiles/unimrcp.xml conf/client-profiles/
cp /usr/local/unimrcp/conf/unimrcpclient.* conf/
cp /usr/local/unimrcp/conf/dirlayout.xml conf/
cp /usr/local/unimrcp/conf/log* conf/
#data
cp -r /usr/local/unimrcp/data/audio/ data/
cp /usr/local/unimrcp/data/one-8kHz.wav data/
cp /usr/local/unimrcp/data/grammar.xml data/
cp /usr/local/unimrcp/data/params_default.txt data/
#lib
cp /usr/local/apr/lib/libapr-1.so.0 lib/
cp /usr/local/apr/lib/libexpat.so.0 lib/
cp /usr/local/lib/libsofia-sip-ua.so.0 lib/
cp /usr/local/unimrcp/lib/libunimrcpclient.so.0 lib/
cp /usr/local/unimrcp/lib/libasrclient.so.0 lib/
zip -r ../mrcp-test-tool.zip .
cd ..