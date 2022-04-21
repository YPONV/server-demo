#include "BitMap.h"

BitMap::BitMap()
{

}

BitMap::BitMap(int size)
{
    array.resize((size>>5)+1);
}

BitMap::~BitMap()
{
}

void BitMap::Init(string str)
{
    string ID = "";
    for(int i = 0; i < str.size(); i ++)
    {
        if(str[i] == '-')//分隔符
        {
            Set(ID);
            ID = "";
        }
        else ID += str[i];
    }
}

int BitMap::Hash1(string str)
{
    long long ans = 0, base = 1331, mod = 10000007;
    for(int i = 0; i <= str.size(); i ++)
    {
        ans = (ans + str[i]) * base % mod;
    }
    return ans;
}
int BitMap::Hash2(string str)
{
    long long ans = 0, base = 1511, mod = 10000007;
    for(int i = 0; i <= str.size(); i ++)
    {
        ans = (ans + str[i]) * base % mod;
    }
    return ans;
}
int BitMap::Hash3(string str)
{
    long long ans = 0, base = 1811, mod = 10000007;
    for(int i = 0; i <= str.size(); i ++)
    {
        ans = (ans + str[i]) * base % mod;
    }
    return ans;
}
bool BitMap::IsExist(string str)
{
    int ans = Hash1(str);
    int index = ans >> 5;
    int n = ans % 32;
   
    if (!( array[index] & (1 << (31 - n) ) ) )//该点未标记了
    {
        return false;
    }
    ans = Hash2(str);
    index = ans >> 5;
    n = ans % 32;

    if (!( array[index] & (1 << (31 - n) ) ) )//该点未标记了
    {
        return false;
    }
    ans = Hash3(str);
    index = ans >> 5;
    n = ans % 32;
 
    if (!( array[index] & (1 << (31 - n) ) ) )//该点未标记了
    {
        return false;
    }
    return true;
}

void BitMap::Set(string str)
{
    int ans = Hash1(str);
    int index = ans >> 5;
    int n = ans % 32;
    size_t a = 1 << (31 - n);
	array[index] |= a;//置为1
 
    ans = Hash2(str);
    index = ans >> 5;
    n = ans % 32;
    a = 1 << (31 - n);
	array[index] |= a;

    ans = Hash3(str);
    index = ans >> 5;
    n = ans % 32;
    a = 1 << (31 - n);
	array[index] |= a;

}
