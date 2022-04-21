#include <iostream>
#include <string.h>
#include <vector>
using namespace std;

class BitMap
{
public:
    BitMap();
    BitMap(int size);
    ~BitMap();

    void Init(string str);
    int Hash1(string str);
    int Hash2(string str);
    int Hash3(string str);
    bool IsExist(string str);
    void Set(string str);

private:

    vector<int> array; //数组
};
