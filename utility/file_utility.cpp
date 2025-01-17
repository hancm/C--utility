#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <string>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
#else
    #include <unistd.h>
    #include <dirent.h>
    #include <sys/param.h>
    #include <sys/stat.h>
    #include <sys/types.h>
#endif
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#include "MyLog.h"
#include "file_utility.h"

using namespace std;

#ifdef _WIN32
typedef std::wstring f_string;
#else
typedef std::string f_string;
#endif

#ifdef _WIN32
#define f_stat      _wstat
typedef struct _stat f_statbuf;
#else
#define f_stat      stat
typedef struct stat f_statbuf;
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
#include <cerrno>
#include <iconv.h>
#include <cstdlib>
#include <cstring>

namespace util
{

/**
 * Helper method for converting from non-UTF-8 encoded strings to UTF-8.
 * Supported LANG values for Linux: see /usr/share/i18n/SUPPORTED.
 * Supported encodings for libiconv: see iconv --list .
 *
 * Note! If non-ASCII characters are used we assume a proper LANG value!!!
 *
 * @param str_in The string to be converted.
 * @param to_UTF: true: 转换输入字符串到utf-8; false: 将输入的utf-8字符串转换为本地字符集字符串
 * @return Returns the input string in UTF-8.
 */
static string convertUTF8(const string &str_in, bool to_UTF)
{
    string charset("C");
    char *env_lang = getenv("LANG");
    if(env_lang && charset.compare(env_lang) != 0)
    {
        charset = env_lang;
        size_t locale_start = charset.rfind(".");
        if(locale_start != string::npos)
            charset = charset.substr(locale_start+1);
    }

    // no conversion needed for UTF-8
    if(charset == "UTF-8" || charset == "utf-8")
        return str_in;

    iconv_t ic_descr = iconv_t(-1);
    try
    {
        ic_descr = to_UTF ? iconv_open("UTF-8", charset.c_str()) : iconv_open(charset.c_str(), "UTF-8");
    }
    catch(exception &) {}

    if(ic_descr == iconv_t(-1))
        return str_in;

    char* inptr = (char*)str_in.c_str();
    size_t inleft = str_in.size();

    string out;
    char outbuf[64];
    char* outptr;
    size_t outleft;

    while(inleft > 0)
    {
        outbuf[0] = '\0';
        outptr = (char *)outbuf;
        outleft = sizeof(outbuf) - sizeof(outbuf[0]);

        size_t result = iconv(ic_descr, &inptr, &inleft, &outptr, &outleft);
        if(result == size_t(-1))
        {
            switch(errno)
            {
            case E2BIG: break;
            case EILSEQ:
            case EINVAL:
            default:
                iconv_close(ic_descr);
                return str_in;
                break;
            }
        }
        *outptr = '\0';
        out += outbuf;
    }
    iconv_close(ic_descr);

    return out;
}
#endif

/**
 * Encodes path to compatible std lib
 * 将utf-8字符串转换为本地字符集
 * @param fileName path
 * @return encoded path
 */
static f_string encodeName(const string &fileName)
{
    if(fileName.empty())
        return f_string();
#if defined(_WIN32)
    int len = MultiByteToWideChar(CP_UTF8, 0, fileName.data(), int(fileName.size()), 0, 0);
    f_string out(len, 0);
    len = MultiByteToWideChar(CP_UTF8, 0, fileName.data(), int(fileName.size()), &out[0], len);
#elif defined(__APPLE__)
    CFMutableStringRef ref = CFStringCreateMutable(0, 0);
    CFStringAppendCString(ref, fileName.c_str(), kCFStringEncodingUTF8);
    CFStringNormalize(ref, kCFStringNormalizationFormD);

    string out(fileName.size() * 2, 0);
    CFStringGetCString(ref, &out[0], out.size(), kCFStringEncodingUTF8);
    CFRelease(ref);
    out.resize(strlen(out.c_str()));
#else
    f_string out = convertUTF8(fileName, false);
#endif
    return out;
}

/**
 * Decodes path from std lib path
 * 将本地文件名称转换为UTF-8字符串
 * @param localFileName path
 * @return decoded path
 */
static string decodeName(const f_string &localFileName)
{
    if(localFileName.empty())
        return string();
#if defined(_WIN32)
    int len = WideCharToMultiByte(CP_UTF8, 0, localFileName.data(), int(localFileName.size()), 0, 0, 0, 0);
    string out(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, localFileName.data(), int(localFileName.size()), &out[0], len, 0, 0);
#elif defined(__APPLE__)
    CFMutableStringRef ref = CFStringCreateMutable(0, 0);
    CFStringAppendCString(ref, localFileName.c_str(), kCFStringEncodingUTF8);
    CFStringNormalize(ref, kCFStringNormalizationFormC);

    string out(localFileName.size() * 2, 0);
    CFStringGetCString(ref, &out[0], out.size(), kCFStringEncodingUTF8);
    CFRelease(ref);
    out.resize(strlen(out.c_str()));
#else
    string out = convertUTF8(localFileName, true);
#endif
    return out;
}

/**
 * @brief 获取当前工作目录
 * @return string
 * @note
 */
string File::cwd()
{
#ifdef _WIN32
#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    return "./";
#endif
    wchar_t *path = _wgetcwd( 0, 0 );
#else
    char *path = getcwd( 0, 0 );
#endif
    string ret;
    if( path )
        ret = decodeName( path );
    free( path );
    return ret;
}

/**
 * @brief 获取环境变量值
 * @param [IN] varname
 * @return string
 * @note
 */
string File::env(const string &varname)
{
#ifdef _WIN32
#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    return string();
#endif
    if(wchar_t *var = _wgetenv(encodeName(varname).c_str()))
#else
    if(char *var = getenv(encodeName(varname).c_str()))
#endif
        return decodeName(var);
    return string();
}

/**
 * Checks whether file exists and is type of file.
 *
 * @param path path to the file, which existence is checked.
 * @return returns true if the file is a file and it exists.
 */
bool File::fileExists(const string &path)
{
    f_statbuf fileInfo;
    f_string _path = encodeName(path);
    if(f_stat(_path.c_str(), &fileInfo) != 0)
        return false;

    // XXX: != S_IFREG
    return !((fileInfo.st_mode & S_IFMT) == S_IFDIR);
}

/**
 * Checks whether directory exists and is type of directory.
 *
 * @param path path to the directory, which existence is checked.
 * @return returns true if the directory is a directory and it exists.
 */
bool File::directoryExists(const string &path)
{
    f_string _path = encodeName(path);
#ifdef _WIN32
    // stat will fail on win32 if path ends with backslash
    if(!_path.empty() && (_path[_path.size() - 1] == L'/' || _path[_path.size() - 1] == L'\\'))
        _path = _path.substr(0, _path.size() - 1);
    // TODO:XXX: "C:" is not a directory, so create recursively will
    // do stack overflow in case first-dir in root doesn't exist.
#endif

    f_statbuf fileInfo;
    if(f_stat(_path.c_str(), &fileInfo) != 0)
        return false;

    return (fileInfo.st_mode & S_IFMT) == S_IFDIR;
}

/**
 * Returns last modified time
 *
 * @param path path which modified time will be checked.
 * @return returns given path modified time.
 */
tm* File::modifiedTime(const string &path)
{
    f_statbuf fileInfo;
    if(f_stat(encodeName(path).c_str(), &fileInfo) != 0)
        return gmtime(0);
    return gmtime((const time_t*)&fileInfo.st_mtime);
}

string File::fileExtension(const std::string &path)
{
    size_t pos = path.find_last_of(".");
    if(pos == string::npos)
        return string();
    string ext = path.substr(pos + 1);
    transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

/**
 * Returns file size
 */
size_t File::fileSize(const string &path)
{
    f_statbuf fileInfo;
    if(f_stat(encodeName(path).c_str(), &fileInfo) != 0)
        return 0;
    return fileInfo.st_size;
}

/**
 * Parses file path and returns file name from file full path.
 *
 * @param path full path of the file.
 * @return returns file name from the file full path in UTF-8.
 */
string File::fileName(const string& path)
{
    size_t pos = path.find_last_of("/\\");
    return pos == string::npos ? path : path.substr(pos + 1);
}

/**
 * Parses file path and returns directory from file full path.
 *
 * @param path full path of the file.
 * @return returns directory part of the file full path.
 */
string File::directory(const string& path)
{
    size_t pos = path.find_last_of("/\\");
    return pos == string::npos ? "" : path.substr(0, pos);
}

/**
 * Creates full path from directory name and relative path.
 *
 * @param directory directory path.
 * @param relativePath relative path.
 * @param unixStyle when set to <code>true</code> returns path with unix path separators,
 *        otherwise returns with operating system specific path separators.
 *        Default value is <code>false</code>.
 * @return returns full path.
 */
string File::path(const string &directory, const string &relativePath)
{
    string dir(directory);
    if(!dir.empty() && (dir[dir.size() - 1] == '/' || dir[dir.size() - 1] == '\\')) {
        dir = dir.substr(0, dir.size() - 1);
    }

    string path = dir + "/" + relativePath;
#ifdef _WIN32
    replace(path.begin(), path.end(), '/', '\\');
#else
    replace(path.begin(), path.end(), '\\', '/');
#endif
    return path;
}

/**
 * @return returns temporary filename.
 */
string File::tempFileName()
{
#ifdef _WIN32
    // requires TMP environment variable to be set
    wchar_t *fileName = _wtempnam(0, 0); // TODO: static buffer, not thread-safe
    if ( !fileName ) {
        LOG_ERROR("Failed to create a temporary file name.");
        return std::string();
    }
#else
     /* 新建文件名和文件，文件名中的XXXXXX将被随机字符串代替，以保证文件名在系统中的唯一性 */
     char fileName[] = "/tmp/temp_file_XXXXXX";
     int fd = mkstemp(fileName);
     if (0 > fd) {
         LOG_ERROR("Failed to create a temporary file name.");
         return std::string();
     }

     /* 文件立刻被unlink，这样只要文件描述符一关闭文件就会被自动删除 */
     unlink(fileName);
     close(fd);
#endif
    string path = decodeName(fileName);
    return path;
}

int File::readFileInfo(const std::string &strFilePath, std::string &fileBuf)
{
    std::ifstream inFile(strFilePath.c_str(), std::ios::in | std::ios::binary);
    if (!inFile) {
        LOG_ERROR("Failed to ifstream file: {}.", strFilePath);
        return -1;
    }

    inFile.seekg(0, std::ios::end);
    size_t fileSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);

    fileBuf.clear();
    fileBuf.resize(fileSize);
    if (!inFile.read(&fileBuf[0], fileSize)) {
        LOG_ERROR("Failed to read file: {}, size: {}.", strFilePath, fileSize);
        inFile.close();
        return -1;
    }

    inFile.close();
    return 0;
}

int File::writeFileInfo(const std::string &strFilePath, const char *fileBuf, size_t fileLen)
{
    std::ofstream outfile(strFilePath.c_str(), std::ios::out | std::ios::binary);
    if (!outfile) {
        LOG_ERROR("Failed to ofstream file: {}.", strFilePath);
        return -1;
    }

    if (!outfile.write(fileBuf, fileLen)) {
        LOG_ERROR("Failed to write file: {}, size: {}.", strFilePath, fileLen);
        outfile.close();
        return -1;
    }

    outfile.close();
    return 0;
}

int File::copyFile(const char *srcFilePath, const char *dstFilePath)
{
    std::string fileBuf;
    if (0 != readFileInfo(srcFilePath, fileBuf)) {
        LOG_ERROR("Failed to read file: {}.", srcFilePath);
        return -1;
    }

    if (0 != writeFileInfo(dstFilePath, &fileBuf[0], fileBuf.size())) {
        LOG_ERROR("Failed to write file: {}.", dstFilePath);
        return -1;
    }

    return 0;
}

/**
 * Creates directory recursively. Also access rights can be omitted. Defaults are 700 in unix.
 *
 * @param path full path of the directory created.
 * @param mode directory access rights, optional parameter, default value 0700 (owner: rwx, group: ---, others: ---)
 * @throws IOException exception is thrown if the directory creation failed.
 */
int File::createDirectory(const string &path)
{
    if(path.empty())
    {
        LOG_ERROR("Can not create directory with no name.");
        return -1;
    }

    if(directoryExists(path))
    {
        LOG_DEBUG("Dir[{}] exists.", path.c_str());
        return 0;
    }

    string parentDir(path);
    if(parentDir[parentDir.size() - 1] == '/' || parentDir[parentDir.size() - 1] == '\\')
    {
        parentDir = parentDir.substr(0, parentDir.size() - 1);
    }
    parentDir = parentDir.substr(0, parentDir.find_last_of("/\\"));

    if(!directoryExists(parentDir))
    {
        createDirectory(parentDir);
    }

#ifdef _WIN32
    int result = _wmkdir(encodeName(path).c_str());
    if ( result ) {
        LOG_DEBUG("Failed to creating directory: {}, failed with errno = {}", path.c_str(), errno);
    } else {
        LOG_DEBUG("Succeed to created directory {}.", path.c_str());
    }
#else
    umask(0);
    // 0775
    int result = mkdir(encodeName(path).c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH );
    if ( result ) {
        LOG_DEBUG("Failed to creating directory: {}, failed with errno = {}", path.c_str(), errno);
    } else {
        LOG_DEBUG("Succeed to created directory {}.", path.c_str());
    }
#endif

    if(result || !directoryExists(path))
    {
        LOG_ERROR("Failed to create directory: {}", path.c_str());
        return -1;
    }

    return 0;
}

/**
 * Returns true if the path is relative
 *
 * @return returns true if the path is relative
 */
bool File::isRelative(const string &path)
{
    f_string _path = encodeName(path);
    if(_path.empty()) return true;
    if(_path[0] == '/') return false;
#ifdef _WIN32
    // drive, e.g. "a:", or UNC root, e.q. "//"
    if( _path.length() >= 2 &&
        ((iswalpha(_path[0]) && _path[1] == ':') ||
         (_path[0] == '/' && _path[1] == '/')) )
        return false;
#endif
    return true;
}

/**
 * Returns list of files (and empty directories, if <code>listEmptyDirectories</code> is set)
 * found in the directory <code>directory</code>.
 *
 * @param directory full path of the directory.
 * @param file vector
 * @param recursion directory
 * @throws IOException throws exception if the directory listing failed.
 */
int File::listFiles(const string& directory, std::vector<std::string> &files, bool bRecurison)
{
#ifdef _POSIX_VERSION
    string _directory = encodeName(directory);
    DIR* pDir = opendir(_directory.c_str());
    if(!pDir) {
        LOG_ERROR("Failed to open directory '%s'", _directory.c_str());
        return -1;
    }

    char fullPath[MAXPATHLEN];
    struct stat info;
    dirent* entry;
    while((entry = readdir(pDir)) != NULL)
    {
        if(string(".").compare(entry->d_name) == 0
        || string("..").compare(entry->d_name) == 0) {
            continue;
        }

        sprintf(fullPath, "%s/%s", _directory.c_str(), entry->d_name);
        if(entry->d_type == 0x08 || (lstat(fullPath, &info) != 0 && S_ISREG(info.st_mode))) {
            // 文件
            files.push_back(path(directory, decodeName(entry->d_name)));
        } else if(bRecurison && entry->d_type & DT_DIR) {
            // 目录
            listFiles(fullPath, files, bRecurison);
        }
    }

    closedir(pDir);
#else
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    WIN32_FIND_DATAW findFileData;
    HANDLE hFind = NULL;

    if ( directory.size() > MAX_PATH )
    {
        // MSDN: "Relative paths are limited to MAX_PATH characters." - can this be true?
        LOG_ERROR("Directory path '%s' exceeds the limit %d", directory.c_str(), MAX_PATH);
        return -1;
    }

    wstring findPattern = encodeName(directory + "\\*");
    hFind = ::FindFirstFileW(findPattern.c_str(), &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Listing contents of directory '%s' failed with LOG_ERROR %d", directory.c_str(), ::GetLastLOG_ERROR());
        return -1;
    }

    do
    {
        wstring fileName(findFileData.cFileName);
        if ( fileName == L"." || fileName == L".." )
            continue; // skip those too

        if(!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            files.push_back(path(directory, decodeName(fileName)));
        } else if (bRecurison) {
            listFiles(path(directory, decodeName(fileName)), files);
        }
    } while ( ::FindNextFileW(hFind, &findFileData) != FALSE );

    // double-check for LOG_ERRORs
    if ( ::GetLastLOG_ERROR() != LOG_ERROR_NO_MORE_FILES ) {
        LOG_ERROR("Listing contents of directory '%s' failed with LOG_ERROR %d", directory.c_str(), ::GetLastLOG_ERROR());
        ::FindClose(hFind);
        return -1;
    }

    ::FindClose(hFind);
#endif
#endif
    return 0;
}

/**
 * Constructs the full file path in the format "file:///fullpath" in URI encoding.
 *
 * @param fullDirectory full directory path to the relativeFilePath
 * @param relativeFilePath file name to be appended to the full path
 * @return full file path in the format "file:///fullpath" in URI encoding.
 */

string File::fullPathUrl(const string &path)
{
    string result = path;
    // Under windows replace the path delimiters
#ifdef _WIN32
    replace(result.begin(), result.end(), '\\', '/');
    return "file:///" + File::toUri(result);
#else
    return "file://" + File::toUri(result);
#endif
}

bool File::removeFile(const string &path)
{
#ifdef _WIN32
    return _wremove(encodeName(path).c_str()) == 0;
#else
    return remove(encodeName(path).c_str()) == 0;
#endif
}

/**
 * Helper method for converting strings with non-ascii characters to the URI format (%HH for each non-ascii character).
 *
 * Not converting:
 * (From RFC 2396 "URI Generic Syntax")
 * reserved = ";" | "/" | "?" | ":" | "@" | "&" | "=" | "+" | "$" | ","
 * mark     = "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
 * @param str_in the string to be converted
 * @return the string converted to the URI format
 */
string File::toUri(const string &path)
{
    static string legal_chars = "-_.!~*'();/?:@&=+$,";
    ostringstream dst;
    for(string::const_iterator i = path.begin(); i != path.end(); ++i)
    {
        if( ((*i >= 'A' && *i <= 'Z') || (*i >= 'a' && *i <= 'z') || (*i >= '0' && *i <= '9')) ||
            legal_chars.find(*i) != string::npos )
            dst << *i;
        else
            dst << '%' << hex << uppercase << (static_cast<int>(*i) & 0xFF);
    }
    return dst.str();
}

/**
 * Helper method for converting strings with non-ascii characters to the URI format (%HH for each non-ascii character).
 *
 * Not converting:
 * (From RFC  RFC 3986 "URI Generic Syntax")
 * unreserved    = ALPHA / DIGIT / “-” / “.” / “_” / “~”
 * gen-delims = “:” / “/” / “?” / “#” / “[” / “]” / “@”
 * sub-delims    = "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="
 *
 *  3.3. Path
 * pchar         = unreserved / pct-encoded / sub-delims / ":" / "@"
 * We also encode sub-delims and ":" "@" to be safe
 *
 * @param str_in the string to be converted
 * @return the string converted to the URI format
 */
string File::toUriPath(const string &path)
{
    static string unreserved = "-._~/";
    ostringstream dst;
    for(string::const_iterator i = path.begin(); i != path.end(); ++i)
    {
        if( ((*i >= 'A' && *i <= 'Z') || (*i >= 'a' && *i <= 'z') || (*i >= '0' && *i <= '9')) ||
            unreserved.find(*i) != string::npos )
            dst << *i;
        else
            dst << '%' << hex << uppercase << (static_cast<int>(*i) & 0xFF);
    }
    return dst.str();
}

string File::fromUriPath(const string &path)
{
    string ret;
    char data[] = "0x00";
    for(string::const_iterator i = path.begin(); i != path.end(); ++i)
    {
        if(*i == '%' && (std::distance(i, path.end()) > 2) && isxdigit(*(i+1)) && isxdigit(*(i+2)))
        {
            data[2] = *(++i);
            data[3] = *(++i);
            ret += static_cast<char>(strtoul(data, 0, 16));
        }
        else {
            ret += *i;
        }
    }
    return ret;
}

vector<unsigned char> File::hexToBin(const string &in)
{
    vector<unsigned char> out;
    char data[] = "00";
    for(string::const_iterator i = in.cbegin(); distance(i, in.cend()) >= 2;)
    {
        data[0] = *(i++);
        data[1] = *(i++);
        out.push_back(static_cast<unsigned char>(strtoul(data, 0, 16)));
    }
    return out;
}

} /* namespace util */