#include "httprequest.h"

using namespace std;

const std::unordered_set<std::string> HttpRequest::default_html{
    "index", "register", "login",
    "welcome", "vedio", "picture",
};

const std::unordered_map<std::string,int> HttpRequest::default_html_tags{
    {"/register.html", 0},
    {"/login.html", 1},
};


int HttpRequest::converHex(char ch)
{
    if (ch >= 'A' && ch <= 'F')
        return (ch - 'A' + 10);
    if (ch >= 'a' && ch <= 'f')
        return (ch - 'a' + 10);

    // const char* temp =(const char*)(ch);
    // return atoi(temp);
    
    return ch;
}

bool HttpRequest::userVerify(const char* dbName,const std::string&name, const std::string& pwd,bool isLogin)
{
    if (name.empty() || pwd.empty())
        return false;
    
    MYSQL* sql = nullptr;
    sqlConnRAII(&sql, SqlConnPool::instance());
    assert(sql);

    bool flag = false;
    unsigned int j = 0;
    char order[256] = {0};
    MYSQL_FIELD* fields = nullptr;
    MYSQL_RES *res = nullptr;

    if (!isLogin)
    {
        flag = true;
    }

    // 查询密码
    snprintf(order, 256, 
    "SELECT uername, passwd FROM %s where username = '%s' LIMIT 1",
        dbName, name.c_str());

    LOG_DEBUG("%s", order);

    if (mysql_query(sql, order))
    {
        mysql_free_result(res);
        return false;
    }

    while (MYSQL_ROW row = (mysql_fetch_row(res)))
    {
        LOG_DEBUG("MYSQL ROW : %s %s", row[0], row[1]);
        string passwd(row[1]);
        
        if (isLogin)
        {
            if (pwd == passwd)
            {
                flag = true;
            }
            else 
            {
                flag = false;
                LOG_DEBUG("pwd error");
            }
        }
        else 
        {
            flag = false;
            LOG_DEBUG("user name was used");
        }
    }

    mysql_free_result(res);

    if (!isLogin && flag == true)
    {
        LOG_DEBUG("register!");
        bzero(order, sizeof(order));
        snprintf(order, sizeof(order), 
        "INSERT INTO %s(username, passwd) VALUES('%s', '%s')", 
        dbName, name.c_str(), pwd.c_str());

        LOG_DEBUG("%s", order);
        if (mysql_query(sql ,order))
        {
            LOG_DEBUG("insert error : %s", mysql_error(sql) );
            flag = false;
        }
        else 
            flag = true;
    }

    SqlConnPool::instance()->freeConn(sql);
    LOG_DEBUG("userVerify success!");
    return flag;
}


/************************************************/

bool HttpRequest::parseRequestLine(const std::string& line)
{
    std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch subMatch;
    if (regex_match(line, subMatch, patten))
    {
        hq_method = subMatch[1];
        hq_path = subMatch[2];
        hq_version = subMatch[3];
        state = PARSE_STATE::HEADERS;

        return true;
    }

    LOG_ERROR("RequestLine Error : %s", line.c_str());;
    return false;
}

void HttpRequest::parseHeader(const std::string& line)
{
    std::regex patten("^([^:]*): ?(.*)$");
    std::smatch subMatch;

    if (regex_match(line, subMatch, patten))
        hq_header[subMatch[1]] = subMatch[2];
    else 
        state = PARSE_STATE::BODY;
}

void HttpRequest::paresBody(const std::string& line){
    hq_body = line;
    parsePost();
    state = PARSE_STATE::FINISH;
    LOG_DEBUG("Body : %s, len : %d ", line.c_str(), line.size());

}

void HttpRequest::parsePath()
{
    if ("/" == hq_path)
        hq_path = "/index.html";
    else 
    {
        for(auto & item : default_html)
        {
            if (item == hq_path)
            {
                hq_path += ".html";
                break;
            }

        }
    }
}

void HttpRequest::parseFromUrlencoded()
{
    if (hq_body.size() == 0)
        return;

    std::string key , value;
    int num = 0;
    size_t n = hq_body.size();
    size_t i = 0, j= 0;

    for(; i < n; ++i)
    {
        auto ch = hq_body[i];
        switch(ch)
        {
            case '=':
                key = hq_body.substr(j, i-j);
                j = i + 1;
                break;
            case '+':
                hq_body[i] = ' ';
                break;
            
            case '%':
                num = converHex(hq_body[i+1])*16 + converHex(hq_body[i+2]);
                hq_body[i+2] = num % 10 + '0';
                hq_body[i+1] = num / 10 + '0';
                i += 2;
                break;
            case '&':
                value = hq_body.substr(j, i-j);
                j = i + 1;
                hq_post[key] = value;
                LOG_DEBUG("%s:%s", key.c_str(), value.c_str());
                break;
            default:
                break;
            
        }
    }

    assert(j <= i);
    if (hq_post.count(key) == 0 && j < i)
    {
        value = hq_body.substr(j, i-j);
        hq_post[key] = value;
    }
}

void HttpRequest::parsePost()
{
    if (hq_method == "POST"  && hq_header["Content-Type"] == "application/x-www-form-urlencoded")
    {
        parseFromUrlencoded();
        if (default_html_tags.count(hq_path))
        {
            int tag = default_html_tags.find(hq_path)->second;
            LOG_DEBUG("tag : %d", tag);
            if (0 == tag || 1 == tag)
            {
                bool isLogin = (1 == tag);
                if (userVerify(hq_post["username"], hq_post["password"], isLogin))
                {
                    hq_path = "/welcome.html";
                }
                else 
                {
                    hq_path = "/error.html";
                }
            }
        }
    }
}



/************************************************/

void HttpRequest::init(){
    hq_method = hq_path = hq_version = hq_body = "";
    state = PARSE_STATE::REQUEST_LINE;
    hq_header.clear();
    hq_post.clear();
}

bool HttpRequest::parse(Buffer& buff)
{
    // 回车键换行符
    const char CRLF[] = "\r\n";
    if (buff.readableBytes() <= 0)
    {
        return false;
    }

    while (buff.readableBytes() && state != PARSE_STATE::FINISH)
    {
        const char* lineEnd = search(buff.peek(), buff.beginWriteConst(), CRLF, CRLF+2);

        std::string line(buff.peek(), lineEnd);

        switch(state)        
        {
            case PARSE_STATE::REQUEST_LINE:
                if (!parseRequestLine(line))
                    return false;
                parsePath();
                break;
            case PARSE_STATE::HEADERS:
                parseHeader(line);
                if (buff.readableBytes() <= 2)
                    state = PARSE_STATE::FINISH;
                break;
            case PARSE_STATE::BODY:
                paresBody(line);
                break;

            default:
                break;
        }

        if (buff.beginWrite() == lineEnd)
            break;
        
        buff.retrieveUntil(lineEnd+2);
    }

    LOG_DEBUG("[%s], [%s], [%s]", hq_method.c_str(), hq_path.c_str(), hq_version.c_str());

    return true;    
}

inline std::string HttpRequest::path()const{
    return hq_path;
}

inline std::string&  HttpRequest::path()
{
    return hq_path;
}

inline std::string HttpRequest::method() const
{
    return hq_method;
}

inline std::string HttpRequest::version() const{
    return hq_version;
}

inline std::string HttpRequest::getPost(const std::string& key) const{
    // assert("" != key);
    assert(!key.empty());
    if (hq_post.count(key) == 1)
        return hq_post.find(key)->second;
    return "";
}

inline std::string HttpRequest::getPost(const char* key) const{
    assert(nullptr != key);
    if (hq_post.count(key) == 1)
        return hq_post.find(key)->second;

    return "";
}