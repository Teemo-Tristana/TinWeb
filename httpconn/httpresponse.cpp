#include "httpresponse.h"

const std::unordered_map<std::string, std::string> HttpResponse::suffix_type=
{
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

const std::unordered_map<int, std::string> HttpResponse::code_status=
{
    {200, "OK"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"},
};

const std::unordered_map<int, std::string> HttpResponse::code_path=
{
    {400, "./400.html"},
    {403, "./403.html"},
    {404, "./404.html"},
};

/*********************************************************/

void HttpResponse::addStateLine(Buffer& buff)
{
    std::string status;
    if (code_status.count(hp_code) == 1)
    {
        status = code_status.find(hp_code)->second;
    }
    else 
    {
        hp_code = 400;
        status = code_status.find(400)->second;
    }

    buff.append("HTTP/1.1 " + std::to_string(hp_code) +" " + status +"\r\n");
}

void HttpResponse::addHeader(Buffer& buff)
{
    buff.append("Connection: ");
    if (hp_isKeepAlive)
    {
        buff.append("keep-alive\r\n");
        buff.append("keep-alive: max=6, timeout=120\r\n");
    }
    else 
    {
        buff.append("close\r\n");
    }

    buff.append("Content-type: " + getFileType() +"\r\n");
}

void HttpResponse::addContent(Buffer& buff)
{
    int srcFd = open((hp_srcDir + hp_path).data(), O_RDONLY);
    if (srcFd < 0)
    {
        errorContent(buff, "File NotFound!");
        return;
    }

    // 将文件映射到内存提高文件的访问速度、 MAP_PRIVATE 建立一改写入时拷贝的私有映射
    LOG_DEBUG("file path %s", (hp_srcDir + hp_path).data());
    int* mmRet =(int*)mmap(0, mmFileStat.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if (-1 == *mmRet)
    {
        errorContent(buff, "File Node find");
    }

    mmFile = (char*)mmRet;
    close(srcFd);
    buff.append("Content-length: " + std::to_string(mmFileStat.st_size) +"\r\n\r\n");
}

void HttpResponse::errorHtml()
{
    if (code_path.count(hp_code) == 1)
    {
        hp_path = code_path.find(hp_code)->second;
        stat((hp_srcDir + hp_path).data(), &mmFileStat);
    }
}

std::string HttpResponse::getFileType()
{
    std::string::size_type idx = hp_path.find_last_of('.');
    if (idx == std::string::npos)
    {
        return "text/plain";
    }

    std::string suffix = hp_path.substr(idx);
    if (1 == suffix_type.count(suffix))
    {
        return suffix_type.find(suffix)->second;
    }

    return "text/plain";
}

/*********************************************************/

HttpResponse::HttpResponse(): hp_code(0), hp_isKeepAlive(false), hp_path(""), 
                              hp_srcDir(""), mmFile(nullptr), mmFileStat({0}){}

HttpResponse::~HttpResponse()
{
    unmapFile();
}


void HttpResponse::init(const std::string& srcDir, std::string path, bool isKeepAlive, int code)
{
    assert(!srcDir.empty());
    if (mmFile)
        unmapFile();

    hp_code = code;
    hp_isKeepAlive = isKeepAlive;
    hp_path = path;
    hp_srcDir = srcDir;
    mmFile = nullptr;
    mmFileStat = {0};
}

void HttpResponse::makeResponse(Buffer& buff)
{
    if (stat((hp_srcDir + hp_path).data(), &mmFileStat) < 0 || S_ISDIR(mmFileStat.st_mode))
    {
        hp_code = 404;
    }
    else if (!(mmFileStat.st_mode & S_IROTH))
    {
        hp_code = 403;
    }
    else if (-1 == hp_code)
    {
        hp_code = 200;
    }

    errorHtml();
    addStateLine(buff);
    addHeader(buff);
    addContent(buff);
}

void HttpResponse::unmapFile()
{
    if (mmFile)
    {
        munmap(mmFile, mmFileStat.st_size);
        mmFile = nullptr;
    }
}

inline char* HttpResponse::file()
{
    return mmFile;
}

inline size_t HttpResponse::fileLen()
{
    return mmFileStat.st_size;
}


void HttpResponse::errorContent(Buffer& buff, std::string message)
{
    std::string body;
    std::string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(1 == code_status.count(hp_code)) {
        status = code_status.find(hp_code)->second;
    } else {
        status = "Bad Request";
    }
    body += std::to_string(hp_code) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
    buff.append(body);
}

inline int HttpResponse::code() const
{
    return hp_code;
}