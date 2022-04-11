#include "CSql.h"

CSql::CSql()
{
    host = "localhost";
    user = "game";
    pwd = "game123u";
    dbName = "gamedatabase";
    port=8080;
    Sql_Connect();
}

CSql::~CSql()
{
    if(sql != NULL)  //关闭数据连接
    {
        mysql_close(sql);
    }
}

void CSql::Sql_Connect()
{
    sql = mysql_init(sql);
	sql = mysql_real_connect(sql, host, user, pwd, dbName, port, nullptr, 0);
	if(sql == NULL)
	{
        printf("Mysql Error\n");
	}
}

void CSql::prepare(string& name, string& password)
{
    //设置插入的类型和长度等信息
    memset(params, 0, sizeof(params));
    params[0].buffer_type = MYSQL_TYPE_STRING;
    params[0].buffer = const_cast<char *>(name.c_str());
    params[0].buffer_length = name.size();
    params[1].buffer_type = MYSQL_TYPE_STRING;
    params[1].buffer = const_cast<char *>(password.c_str());
    params[1].buffer_length = password.size();
}
void CSql::prepare1(string& UID, string& name, string& password)
{
    //设置插入的类型和长度等信息
    memset(params, 0, sizeof(params));
    params[0].buffer_type = MYSQL_TYPE_STRING;
    params[0].buffer = const_cast<char *>(UID.c_str());
    params[0].buffer_length = UID.size();
    params[1].buffer_type = MYSQL_TYPE_STRING;
    params[1].buffer = const_cast<char *>(name.c_str());
    params[1].buffer_length = name.size();
    params[2].buffer_type = MYSQL_TYPE_STRING;
    params[2].buffer = const_cast<char *>(password.c_str());
    params[2].buffer_length = password.size();
}

int CSql::Sql_Write(string& UID, string& name, string& password)
{
    st = mysql_stmt_init(sql);
    const char *query = "insert into player values(?, ?, ?);";
    if(mysql_stmt_prepare(st, query, strlen(query)))
    {
        fprintf(stderr, "mysql_stmt_prepare: %s\n", mysql_error(sql));
        exit(0);
    }
    prepare1(UID, name, password);
    mysql_stmt_bind_param(st, params); 
    if (mysql_stmt_execute(st))//执行与语句句柄相关的预处理
    {
        fprintf(stderr, " mysql_stmt_execute(), failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(st));
        exit(0);
    }        
    if (mysql_stmt_close(st))
    {
        fprintf(stderr, " failed while closing the statement\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(st));
        exit(0);
    }
}

int CSql::Sql_Query(string& UID, string& password)
{
    st = mysql_stmt_init(sql);
    const char *query = "select * from player where uid=? and password=?;";
    if(mysql_stmt_prepare(st, query, strlen(query)))
    {
        fprintf(stderr, "mysql_stmt_prepare: %s\n", mysql_error(sql));
        exit(0);
    }
    prepare(UID, password);
    mysql_stmt_bind_param(st, params);    
    //MYSQL_RES* prepare_meta_result = mysql_stmt_result_metadata(st); 
    //int column_count= mysql_num_fields(prepare_meta_result);

    if (mysql_stmt_execute(st))//执行与语句句柄相关的预处理
    {
        fprintf(stderr, " mysql_stmt_execute(), failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(st));
        exit(0);
    }
    //对结果集进行缓冲处理
    if(mysql_stmt_store_result(st))
    {
        fprintf(stderr, " mysql_stmt_store_result() failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(st));
        exit(0);
    }
    int row_count = mysql_stmt_affected_rows(st);//受影响的行
    if (mysql_stmt_close(st))
    {
        fprintf(stderr, " failed while closing the statement\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(st));
        exit(0);
    } 
    if(row_count == 1)//查询成功
    {
        return 1;
    }
    else return 0;
}

void CSql::Sql_Update(string& name, string& password)
{
    st = mysql_stmt_init(sql);
    const char *query = "update player set password=? where name=?;";
    if(mysql_stmt_prepare(st, query, strlen(query)))
    {
        fprintf(stderr, "mysql_stmt_prepare: %s\n", mysql_error(sql));
        exit(0);
    }
    prepare(password, name);
    mysql_stmt_bind_param(st, params); 
    if (mysql_stmt_execute(st))//执行与语句句柄相关的预处理
    {
        fprintf(stderr, " mysql_stmt_execute(), failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(st));
        exit(0);
    }        
    if (mysql_stmt_close(st))
    {
        fprintf(stderr, " failed while closing the statement\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(st));
        exit(0);
    }
}

void CSql::Sql_Delete(string& name,string &password)
{
    st = mysql_stmt_init(sql);
    const char *query = "delete from player where name=? and password=?;";
    if(mysql_stmt_prepare(st, query, strlen(query)))
    {
        fprintf(stderr, "mysql_stmt_prepare: %s\n", mysql_error(sql));
        exit(0);
    }
    prepare(name, password);
    mysql_stmt_bind_param(st, params); 
    if (mysql_stmt_execute(st))//执行与语句句柄相关的预处理
    {
        fprintf(stderr, " mysql_stmt_execute(), failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(st));
        exit(0);
    }        
    if (mysql_stmt_close(st))
    {
        fprintf(stderr, " failed while closing the statement\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(st));
        exit(0);
    }
}