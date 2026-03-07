#include "config.h"


Config::Config() {
    //端口号默认9006
    PORT = 9006;
    //线程池内的线程数量,默认8
    thread_num = 8;
    //关闭日志,默认不关闭
    close_log = 0;

}
//c++11以后局部懒汉不需要加锁
Config * Config::get_instance()
{
    static Config config;
    return &config;
}


void Config::parse_arg(int argc,char * argv[] )
{
    int opt;
    const char * str="p:l:m:o:t:c:a:";

    while( (opt=getopt(argc,argv,str) )!=-1 )
    {
        switch(opt)
        {
            case 'p': //自定义端口号
            {
                //optarg 是一个指向当前选项参数的指针，通常用于处理带参数的选项。默认后台会更新这个东西
                PORT = atoi(optarg);
                break;
            }
            case 't':  //线程数量
            {
                thread_num = atoi(optarg);
                break;
            }
            case 'c':    //是否关闭日志
            {
                close_log = atoi(optarg);
                break;
            }
            default:
                break;
        }

    }

}