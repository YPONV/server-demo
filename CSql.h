#include <iostream>
#include <mysql/mysql.h>
#include <string.h>
using namespace std;

class CSql
{
public:
    CSql();
    ~CSql();

    const char* host;
    const char* user;
    const char* pwd;
    const char* dbName;
    int port;
    //预编译sql语句，防止sql注入。
    MYSQL_STMT * st;
    MYSQL_BIND params[3];

    MYSQL *sql = nullptr;
    //连接数据库
    void Sql_Connect();
    //写入
    int Sql_Write(string& UID, string& name, string& password);
    //查询
    int Sql_Query(string& UID, string& password);
    //修改
    void Sql_Update(string& UID, string& password);
    //删除
    void Sql_Delete(string& UID, string& password);
    //准备需要绑定的数组
    void prepare(string& UID, string& password);
    void prepare1(string& UID, string& name, string& password);

    string GetAllID();
};