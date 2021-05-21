all: backend frontend

backend: backend.cpp payload.pb.cc
	g++ -o backend backend.cpp payload.pb.cc -lpthread -lprotobuf

frontend: frontend.cpp payload.pb.cc
	g++ -o frontend frontend.cpp payload.pb.cc -lpthread -lprotobuf

payload.pb.cc: payload.proto
	protoc -I=. --cpp_out=. payload.proto
