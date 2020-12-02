#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include <unordered_map>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "../log/log.h"
#include "../buffer/buffer.h"

/**
 * http响应类： HttpResponse
 * 
 *   文件结构 struct stat、
 *   C++11： unordered_map、string
 *   http ：相关知识点比如状态码、响应格式、响应组成
 *   文件映射与零拷贝
 *   html基本用法
*/

class HttpResponse{
  public:
    HttpResponse();
    ~HttpResponse();

    void init(const std::string& srcDir, std::string path, bool isKeepAlive=false, int code = -1);
    void makeResponse(Buffer& buff);
    void unmapFile();
    char* file();
    size_t fileLen();
    void errorContent(Buffer& buff, std::string message);
    int code()const;

  private:
    void addStateLine(Buffer& buff);
    void addHeader(Buffer& buff);
    void addContent(Buffer& buff);

    void errorHtml();
    std::string getFileType();

  private:
    
    int hp_code ;  
    bool hp_isKeepAlive;

    std::string hp_path;
    std::string hp_srcDir;

    char* mmFile;
    struct stat mmFileStat;

    static const std::unordered_map<std::string, std::string> suffix_type;
    static const std::unordered_map<int, std::string> code_status;
    static const std::unordered_map<int, std::string> code_path;
};


#endif