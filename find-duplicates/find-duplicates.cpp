// find-duplicates.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "crypt.h"
#include "string_utils.h"

#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

class Config
{
public:
    const std::vector<tstring>& get_directories();
    void add_dir(const TCHAR* dir_name);
    virtual void initialize() = 0;
private:
    std::vector<tstring> directories_to_scan_;
};

void Config::add_dir(const TCHAR* dir_name)
{
    directories_to_scan_.push_back(dir_name);
}

const std::vector<tstring>& Config::get_directories()
{
    return directories_to_scan_;
}

class FileConfig: public Config
{
public:
    FileConfig(const TCHAR* file_name);
    void initialize();
private:
    tstring file_name_;
};

FileConfig::FileConfig(const TCHAR* file_name):
    file_name_(file_name)
{
}

void FileConfig::initialize()
{
    std::ifstream ifile;
    ifile.open(file_name_);

    if (!ifile.is_open())
    {
        std::wcerr << _T("configuration \"") << file_name_.c_str() << _T("\" could not be open") << std::endl;
        return;
    }

    while (!ifile.eof())
    {
        std::string str;
        std::getline(ifile, str);
        if (!str.empty())
        {
            tstring tstr = string_utils::to_tstring(str.c_str(), CP_UTF8);
            if (tstr.length() > 0 && tstr[tstr.length() - 1] == _T('\\'))
                tstr = tstr.substr(0, tstr.length() - 1);

            add_dir(tstr.c_str());
        }
    }
    ifile.close();
}

static
void print_windows_error(DWORD error, const TCHAR* msg)
{
    if (error == ERROR_SUCCESS)
        return;

    TCHAR buf[256];
    DWORD len = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
                                buf, (sizeof(buf) / sizeof(buf[0])), NULL);
    for (int i = 0; i < 2 && len > 0; ++i)
    {
        --len;
        if (buf[len] == 10 || buf[len] == 13)
            buf[len] = 0;
    }
    if (msg != NULL)
        std::wcerr << msg << _T(" ");
    std::wcerr << error << _T(" - \"") << buf << _T("\"") << std::endl;
}

static
bool obtain_backup_priviledge()
{
// Obtain backup/restore privilege in case we don't have it
    HANDLE hToken;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        std::wcerr << _T("Unable to OpenProcessToken: ") << ::GetLastError() << std::endl;
        return false;
    }
    std::unique_ptr<HANDLE, std::function<void (HANDLE*)> > close_guard(&hToken, [](HANDLE* h){if (*h != INVALID_HANDLE_VALUE) ::CloseHandle(*h);});
    TOKEN_PRIVILEGES tp = {0,};
    if (!::LookupPrivilegeValue(NULL, SE_BACKUP_NAME, &tp.Privileges[0].Luid))
    {
        std::wcerr << _T("Unable to LookupPrivilegeValue: ") << ::GetLastError() << std::endl;
        return false;
    }
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    TOKEN_PRIVILEGES old_tp = {0,};
    DWORD changed = 0;

    ::AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(old_tp), &old_tp, &changed);
    std::wcerr << _T("Changed privilege count ") << old_tp.PrivilegeCount << std::endl;
    if (old_tp.PrivilegeCount != 0)
        std::wcerr << _T("Previous Luid: ") << old_tp.Privileges[0].Luid.HighPart << _T(" ") << old_tp.Privileges[0].Luid.LowPart  << _T(" attributes: ") << old_tp.Privileges[0].Attributes << std::endl;

    print_windows_error(::GetLastError(), _T("AdjustTokenPrivileges:"));

    return true;
}

struct FileKey
{
    __int64 size_;
    __int64 sig_;

    bool operator < (const FileKey& other) const
    {
        if (size_ < other.size_)
            return true;
        else if (size_ > other.size_)
            return false;

        return sig_ < other.sig_; 
    }
};

struct FileDesc
{
    std::string path_;
};

struct FileSetKey
{
    md5_hash hash_;
    std::string name_;

    bool operator < (const FileSetKey& other) const
    {
        const int hash_cmp = memcmp(hash_.hash, other.hash_.hash, sizeof(hash_.hash));
        if (hash_cmp < 0)
            return true;
        else if (hash_cmp > 0)
            return false;

        return name_ < other.name_;
    }
};

class FsIterator
{
public:
    FsIterator();

    void iterate_dir(const TCHAR* cname);
    void print_results();
private:
    void add_file(__int64 size, __int64 sig, const std::string& name, const std::string& path);
    void calculate_hash(__int64 size, const std::string& path, md5_hash& hash);
    typedef std::map<FileKey, std::multimap<FileSetKey, FileDesc> > Files;
    
    Files files_;
    std::vector<char> buffer_;
};

FsIterator::FsIterator()
{
    buffer_.resize(1024*64);
}

void FsIterator::calculate_hash(__int64 size, const std::string& path, md5_hash& hash)
{
    md5_state_t state;

    md5_init(&state);

    std::ifstream ifile;
    const tstring wpath = string_utils::to_tstring(path.c_str(), CP_UTF8);
    ifile.open(wpath, std::ios_base::in | std::ios_base::binary);
    if (!ifile.is_open())
    {
        std::cerr << __FUNCTION__ << ": warning - file \"" << path << "\" could not be open" << std::endl;
        hash.hash[0] = 153;
        return;
    }

    __int64 total_bytes = 0;

    for (; !ifile.eof();)
    {
        ifile.read(&buffer_[0], buffer_.size());
        const std::streamsize read_bytes = ifile.gcount();
        md5_append(&state, reinterpret_cast<const BYTE*>(&buffer_[0]), static_cast<size_t>(read_bytes));
        total_bytes += read_bytes;
    }
    ifile.close();

    md5_finish(&state, hash.hash);

    if (total_bytes != size)
        std::cerr << "Warning: " << total_bytes << " read from file \"" << path << "\" with initial size " << size << std::endl; 
}

void FsIterator::add_file(__int64 size, __int64 sig, const std::string& name, const std::string& path)
{
    const FileKey key = {size, sig};
    Files::iterator i = files_.find(key);
    static const md5_hash zero_hash = {0,};
    FileDesc fd = {path};
    FileSetKey fsk = {zero_hash, name};

    if (i == files_.end())
    {
        std::multimap<FileSetKey, FileDesc> mm;
        i = files_.insert(std::pair<FileKey, std::multimap<FileSetKey, FileDesc> >(key, mm)).first;
    }
    else
    {
        size_t recalculated = 0;
        size_t total = 0;
        for (std::multimap<FileSetKey, FileDesc>::iterator mmi = i->second.begin(); mmi != i->second.end(); ++total)
        {
            if (memcmp(&mmi->first.hash_, &zero_hash, sizeof(zero_hash)) != 0)
                ++mmi;
            else
            {
                FileSetKey fsk = mmi->first;
                const FileDesc fd = mmi->second;

                calculate_hash(i->first.size_, mmi->second.path_, fsk.hash_);

                std::multimap<FileSetKey, FileDesc>::iterator next = mmi;
                ++next;

                i->second.erase(mmi);
                i->second.insert(std::pair<FileSetKey, FileDesc>(fsk, fd));

                ++recalculated;
                mmi = next;
            }
        }
        if ((recalculated != 1 && total == 1) || (recalculated != 0 && total > 1))
            std::cerr << "Warning: " << recalculated << " items for \"" << name << "\" - \"" << path << "\" size: " << size << ", sig: " << std::hex << sig << std::dec << std::endl;

        calculate_hash(size, path, fsk.hash_);
    }
    i->second.insert(std::pair<FileSetKey, FileDesc>(fsk, fd));
}

void FsIterator::print_results()
{
    for (Files::const_iterator ci = files_.begin(); ci != files_.end(); ++ci)
    {
        for (std::multimap<FileSetKey, FileDesc>::const_iterator mmci = ci->second.begin(); mmci != ci->second.end(); ++mmci)
        {
            static const size_t hash_size = sizeof(mmci->first.hash_.hash)/sizeof(mmci->first.hash_.hash[0]);
            char hash[hash_size*2 + 1];
            for (int i = 0; i < hash_size; ++i)
                sprintf(hash + i*2, "%02X", mmci->first.hash_.hash[i]);

            char sig[sizeof(__int64)*2 + 1];
            sprintf(sig, "%016llX", ci->first.sig_);

            std::cout << ci->first.size_ << " " << sig << " " << hash << " ";
            std::cout << "\"" << mmci->first.name_ << "\" \"" << mmci->second.path_ << "\"" << std::endl;
        }
    }
}

void FsIterator::iterate_dir(const TCHAR* cname)
{
    WIN32_FIND_DATA data;
    int next_res = 1;

    tstring name(cname);
    name += _T("\\*");

    HANDLE hFind = ::FindFirstFile(name.c_str(), &data);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        tstring msg(_T("FindFirstFile: "));
        msg += name + _T(" - ");
        print_windows_error(::GetLastError(), msg.c_str());
    }
    else
    {
        std::unique_ptr<HANDLE, std::function<void (HANDLE*)> > close_guard(&hFind, [](HANDLE* h){if (*h != INVALID_HANDLE_VALUE) ::FindClose(*h);});
        for (; hFind != INVALID_HANDLE_VALUE && next_res != 0; next_res = ::FindNextFile(hFind, &data))
        {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            {
//                std::wcout << cname << _T("\\") << data.cFileName;
//                std::wcout << _T(" reparse point");
//                std::wcout << std::endl;
            }
            else if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                if (_tcscmp(data.cFileName, _T(".")) != 0 && _tcscmp(data.cFileName, _T("..")) != 0)
                {
//                    std::wcout << cname << _T("\\") << data.cFileName;
//                    std::wcout << _T(" directory");
//                    std::wcout << std::endl;
                    tstring name(cname);
                    name += _T("\\");
                    name += data.cFileName;
                    iterate_dir(name.c_str());
                }
            }
            else
            {
                std::string name = string_utils::to_ansi_string(data.cFileName, CP_UTF8);
                std::string path = string_utils::to_ansi_string(cname, CP_UTF8) + "\\" + name;
                const __int64 size = (((__int64)data.nFileSizeHigh) << 32) | data.nFileSizeLow;

                std::ifstream ifile;
                const tstring wpath = tstring(cname) + _T("\\") + tstring(data.cFileName);
                ifile.open(wpath, std::ios_base::in | std::ios_base::binary);
                if (!ifile.is_open())
                    std::cerr << "file \"" << path.c_str() << "\" could not be open" << std::endl;
                else
                {
                    __int64 sig = 0;
                    if (size != 0)
                    {
                        const std::streamsize bytes = size < sizeof(sig) ? size : sizeof(sig);
                        const bool before_read = (ifile?true:false);
                        const std::streamsize pos = ifile.tellg();
                        ifile.read((char*)&sig, bytes);
                        const std::streamsize read_bytes = ifile.gcount();
                        if (read_bytes != bytes)
                            std::cerr << pos << " " << before_read << " " << ifile.eof() << " " << (ifile?true:false) << " " << ::GetLastError() << " " << read_bytes << " Unable to read " << bytes << " bytes from " << path << std::endl;

                        add_file(size, sig, name, path);
                    }
                }
                ifile.close();
            }
        }
    }
}

int _tmain(int argc, _TCHAR* argv[])
{
    if (argc < 2)
    {
        std::wcerr << _T("please specify configuration file name") << std::endl;
        return 1;
    }

    obtain_backup_priviledge();

    FileConfig fc(argv[1]);
    fc.initialize();

    FsIterator fsi;

    for (std::vector<tstring>::const_iterator ci = fc.get_directories().begin(); ci != fc.get_directories().end(); ++ci)
    {
        std::wcerr << ci->c_str() << std::endl;
        fsi.iterate_dir(ci->c_str());
    }

    fsi.print_results();

    return 0;
}
