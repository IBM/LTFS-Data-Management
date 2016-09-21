



all:
	protoc --proto_path=.. --cpp_out=. ../ltfsdm.proto
	g++ -ggdb -o client -I. -I.. -I/usr/local/include client.cc -L/usr/local/lib ../LTFSDmComm.cc ltfsdm.pb.cc -lprotobuf
	g++ -ggdb -o server -I. -I.. -I/usr/local/include server.cc -L/usr/local/lib ../LTFSDmComm.cc ltfsdm.pb.cc -lprotobuf

clean:
	rm -f client server ltfsdm.pb.*
