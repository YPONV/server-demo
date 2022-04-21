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

void CSql::prepare(string& UID, string& password)
{
    //设置插入的类型和长度等信息
    memset(params, 0, sizeof(params));
    params[0].buffer_type = MYSQL_TYPE_STRING;
    params[0].buffer = const_cast<char *>(UID.c_str());
    params[0].buffer_length = UID.size();
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
    const char *query = "select * from player where uid=?;";
    if(mysql_stmt_prepare(st, query, strlen(query)))
    {
        fprintf(stderr, "mysql_stmt_prepare: %s\n", mysql_error(sql));
        exit(0);
    }
    memset(params, 0, sizeof(params));
    params[0].buffer_type = MYSQL_TYPE_STRING;
    params[0].buffer = const_cast<char *>(UID.c_str());
    params[0].buffer_length = UID.size();
    mysql_stmt_bind_param(st, params); 

    MYSQL_BIND bind[3];
    memset(bind,0,sizeof bind);
    char str_data1[100], str_data2[100], str_data3[100];
    memset(str_data1,0,sizeof str_data1);
    memset(str_data2,0,sizeof str_data2);
    memset(str_data3,0,sizeof str_data3);
    unsigned long str_length[3];
    bind[0].buffer_type= MYSQL_TYPE_STRING; //设置第2个占位符的属性
    bind[0].buffer= (char *)str_data1;
    bind[0].buffer_length= 64;
    bind[0].is_null= 0;
    bind[0].length= &str_length[0];

    bind[1].buffer_type= MYSQL_TYPE_STRING; //设置第2个占位符的属性
    bind[1].buffer= (char *)str_data2;
    bind[1].buffer_length= 64;
    bind[1].is_null= 0;
    bind[1].length= &str_length[1];

    bind[2].buffer_type= MYSQL_TYPE_STRING; //设置第3个占位符的属性
    bind[2].buffer= (char *)str_data2;
    bind[2].buffer_length= 64;
    bind[2].is_null= 0;
    bind[2].length= &str_length[2];

    if (mysql_stmt_bind_result(st, bind))
    {
        fprintf(stderr, " mysql_stmt_bind_result() failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(st));
        exit(0);
    }
    if (mysql_stmt_execute(st))//执行与语句句柄相关的预处理
    {
        fprintf(stderr, " mysql_stmt_execute(), failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(st));
        exit(0);
    }
    int number = 0;
    while(!mysql_stmt_fetch(st))
    {
        number ++;
        int len = strlen(str_data2);
        for(int i = 0; i < len; i ++)
        {
            password += str_data2[i];
        }
    }
    if(number == 1)return true;
    else return false;
}

void CSql::Sql_Update(string& UID, string& password)
{
    st = mysql_stmt_init(sql);
    const char *query = "update player set password=? where name=?;";
    if(mysql_stmt_prepare(st, query, strlen(query)))
    {
        fprintf(stderr, "mysql_stmt_prepare: %s\n", mysql_error(sql));
        exit(0);
    }
    prepare(password, UID);
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

void CSql::Sql_Delete(string& UID,string &password)
{
    st = mysql_stmt_init(sql);
    const char *query = "delete from player where name=? and password=?;";
    if(mysql_stmt_prepare(st, query, strlen(query)))
    {
        fprintf(stderr, "mysql_stmt_prepare: %s\n", mysql_error(sql));
        exit(0);
    }
    prepare(UID, password);
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
string CSql::GetAllID()
{
    string answer = "";
    const char *query = "select * from player";
    if(mysql_query(sql, query))
	{
		cout << "查询失败" << endl;
	}
	MYSQL_RES *result;
	result = mysql_store_result(sql);
	if(result)
	{
        int row_num , col_num;
		row_num = mysql_num_rows(result);
		col_num = mysql_num_fields(result);
		MYSQL_ROW Row;
		MYSQL_FIELD *fd;
		Row = mysql_fetch_row(result);
		while(Row != NULL)
		{
			uint64_t *m_fields_length = mysql_fetch_lengths(result);
			string str[3];
			for(int i = 0;i < col_num ;i ++ )
			{
				str[i] = std::string(Row[i], (size_t)m_fields_length[i]);
			}
            answer += str[0];
            answer += "-";
			Row = mysql_fetch_row(result);
		}
    }
    else
    {
        printf("error(%d):%s\n", mysql_errno(sql), mysql_error(sql));
    }
    return answer;
}