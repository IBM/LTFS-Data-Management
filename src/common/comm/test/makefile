



all:
	protoc --proto_path=.. --cpp_out=. ../ltfsdm.proto
	g++ -ggdb -o client -I. -I.. -I/opt/local/include client.cc -L/opt/local/lib ../LTFSDmC.cc ltfsdm.pb.cc -lprotobuf
	g++ -ggdb -o server -I. -I.. -I/opt/local/include server.cc -L/opt/local/lib ../LTFSDmC.cc ltfsdm.pb.cc -lprotobuf

clean:
	rm -f client server ltfsdm.pb.*
