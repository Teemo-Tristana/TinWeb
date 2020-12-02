#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <errno.h>
#include <mysql/mysql.h>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../mysqlpool/sqlconnpool.h"
#include "../mysqlpool/sqlconnRAII.h"



/**
 * http请求类： httprequest     
 *      C++ 初始化列表、强枚举、serach() 函数
 *      unorder_set、 unordered_map、 内联函数 inline
 *      正则匹配 regex
 *      MySQL数据库
 *      http请求组成部分、 http请求头部的字段
 *      有限状态机
*/
class HttpRequest{
    public:
        enum struct PARSE_STATE
        {
            REQUEST_LINE,
            HEADERS,
            BODY,
            FINISH,
        };

        enum struct HTTP_CODE: unsigned int
        {
            NO_REQUEST = 0,
            GET_REQUEST,
            BAD_REQUEST,
            NO_RESOURSE,
            FORNIDDENT_REQUEST,
            FILE_REQUEST,
            INTERNAL_ERROR,
            CLOSED_CONNECTION,
        };

    public:
        HttpRequest() { init();}
        ~HttpRequest() = default;

        void init();
        bool parse(Buffer& buff);

        std::string path() const ;
        std::string& path();
        std::string method() const;
        std::string version() const;
        std::string getPost(const std::string& key) const;
        std::string getPost(const char* key) const;

        bool isKeepAlive() const;

    private:
        bool parseRequestLine(const std::string& line);
        void parseHeader(const std::string& line);
        void paresBody(const std::string& line);

        
        void parseFromUrlencoded();
       

        void parsePath();
        void parsePost();

    private:
      PARSE_STATE state;
      
      std::string hq_method, hq_path, hq_version, hq_body;
      std::unordered_map<std::string, std::string> hq_header;
      std::unordered_map<std::string, std::string> hq_post;

      static const std::unordered_set<std::string> default_html;
      static const std::unordered_map<std::string,int> default_html_tags;

      static int converHex(char ch);
       static bool userVerify(const char* dbName, const std::string& name, const std::string& pwd, bool isLogin);

};

#endif