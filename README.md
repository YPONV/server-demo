# server-demo
三组服务器demo

编译代码

GateServer编译：  g++ GateServer.cpp MsgQueue.cpp CEpoll.cpp project.pb.cc -lprotobuf -o GateServer  -std=c++11 -lpthread -I/usr/include/mysql/ -L/usr/lib64/mysql -L/usr/local/lib/ -lmysqlclient -L/usr/local/lib/ -lhiredis

GameServer编译：  g++ GameServer.cpp MsgQueue.cpp CSql.cpp CRedis.cpp project.pb.cc -lprotobuf -o GameServer -std=c++11 -lpthread -I/usr/include/mysql/ -L/usr/lib64/mysql -L/usr/local/lib/ -lmysqlclient -L/usr/local/lib/ -lhiredis

DBServer编译：    g++ DBServer.cpp MsgQueue.cpp CSql.cpp BitMap.cpp CEpoll.cpp CRedis.cpp project.pb.cc -lprotobuf -o DBServer  -std=c++11 -lpthread -I/usr/include/mysql/ -L/usr/lib64/mysql -L/usr/local/lib/ -lmysqlclient -L/usr/local/lib/ -lhiredis

LoginServer编译： g++ LoginServer.cpp MsgQueue.cpp CEpoll.cpp project.pb.cc -lprotobuf -o LoginServer -std=c++11 -lpthread -I/usr/include/mysql/ -L/usr/lib64/mysql -L/usr/local/lib/ -lmysqlclient -L/usr/local/lib/ -lhiredis

运行顺序为：GateServer DBServer LoginServer GameServer
