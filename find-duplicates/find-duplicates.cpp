// find-duplicates.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "crypt.h"
#include "string_utils.h"

#include <process.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

static
void print_windows_error(DWORD error, const char* msg = NULL)
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
        std::cerr << msg << " ";
    std::cerr << error << " - \"" << string_utils::to_ansi_string(buf, CP_UTF8) << "\"" << std::endl;
}

struct DirectoryDesc
{
    tstring dir_;
    std::set<int> drives_;
    int thread_;
};

class Config
{
public:
    const std::vector<tstring> get_directories(int thread_n);
    void add_dir(const TCHAR* dir_name, const std::set<int>& drives);
    virtual void initialize() = 0;
protected:
    std::vector<DirectoryDesc> directories_to_scan_;
};

void Config::add_dir(const TCHAR* dir_name, const std::set<int>& drives)
{
    DirectoryDesc dir = {
        dir_name,
        drives,
        0,
    };
    directories_to_scan_.push_back(dir);
}

const std::vector<tstring> Config::get_directories(int thread_n)
{
    std::vector<tstring> result;

    for (std::vector<DirectoryDesc>::const_iterator i = directories_to_scan_.begin(); i != directories_to_scan_.end(); ++i)
    {
        if (i->thread_ == thread_n)
            result.push_back(i->dir_);
    }
    return result;
}

class FileConfig: public Config
{
public:
    FileConfig(const TCHAR* file_name);
    void initialize();
private:
    void find_drive_groups();
    tstring file_name_;
};

FileConfig::FileConfig(const TCHAR* file_name):
    file_name_(file_name)
{
}

void FileConfig::initialize()
{
/*
{
    std::set<int> s;
    s.insert(1); s.insert(2); s.insert(3); s.insert(4);
    add_dir(_T(""), s); s.clear();

    s.insert(11); s.insert(12); s.insert(13); s.insert(14);
    add_dir(_T(""), s); s.clear();

    s.insert(21); s.insert(22); s.insert(23); s.insert(24);
    add_dir(_T(""), s); s.clear();

    s.insert(1); s.insert(24);
    add_dir(_T(""), s); s.clear();

    find_drive_groups();
    exit(0);
}
*/
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
        str = string_utils::string_trim(str);
        if (!str.empty())
        {
            tstring tstr = string_utils::to_tstring(str.c_str(), CP_UTF8);
            if (tstr.length() > 0 && tstr[tstr.length() - 1] == _T('\\'))
                tstr = tstr.substr(0, tstr.length() - 1);

            std::set<int> drives;
            if (tstr.length() > 1 && ((tstr[0] >= _T('a') && tstr[0] <= _T('z')) || (tstr[0] >= _T('A') && tstr[0] <= _T('Z'))) && tstr[1] == _T(':'))
            {
                tstring drive_path(_T("\\\\.\\"));
                drive_path += tstr.substr(0, 2);

                tstr = _T("\\\\?\\") + tstr;

                HANDLE drive_handle = ::CreateFile(drive_path.c_str(), GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
                if (drive_handle == INVALID_HANDLE_VALUE)
                {
                    print_windows_error(GetLastError());
//                    drives.insert(0);
                }
                else
                {
                    std::unique_ptr<HANDLE, std::function<void (HANDLE*)> > close_guard(&drive_handle, [](HANDLE* h){if (*h != INVALID_HANDLE_VALUE) ::CloseHandle(*h);});
//std::cerr << std::hex << drive_handle << std::dec << std::endl;
                    static const size_t BUF_SIZE = 1024;
                    std::vector<char> buf(BUF_SIZE);
                    VOLUME_DISK_EXTENTS* const disk_extents = reinterpret_cast<VOLUME_DISK_EXTENTS*>(&buf[0]);
                    DWORD size;
                    const BOOL res = ::DeviceIoControl(drive_handle, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                                                       NULL, 0, disk_extents, BUF_SIZE, &size, NULL);
                    if (!res)
                        print_windows_error(GetLastError());
                    else
                    {
std::cerr << "extents " << disk_extents->NumberOfDiskExtents << std::endl;
                        for (size_t extent = 0; extent < disk_extents->NumberOfDiskExtents; ++extent)
                        {
std::cerr << "disk number " << disk_extents->Extents[extent].DiskNumber << std::endl;
                            drives.insert(disk_extents->Extents[extent].DiskNumber);
                        }
                    }
                }
            }
            else if (tstr.length() > 1 && tstr[0] == _T('\\') && tstr[1] == _T('\\'))
                tstr = _T("\\\\?\\UNC\\") + tstr.substr(2);

            add_dir(tstr.c_str(), drives);
        }
    }
    ifile.close();

    find_drive_groups();
}

void FileConfig::find_drive_groups()
{
    for (bool modified = true; modified;)
    {
        modified = false;
        for (std::vector<DirectoryDesc>::iterator i = directories_to_scan_.begin(); !modified && i != directories_to_scan_.end(); ++i)
        {
            for (std::vector<DirectoryDesc>::iterator si = directories_to_scan_.begin(); !modified && si != directories_to_scan_.end(); ++si)
            {
                if (i != si && i->drives_ != si->drives_)
                {
                    std::vector<int> intersection;
                    std::set_intersection(i->drives_.begin(), i->drives_.end(), si->drives_.begin(), si->drives_.end(),
                                          std::inserter(intersection, intersection.begin()));
                    if (!intersection.empty())
                    {
                        std::vector<int> v_union;
                        std::set_union(i->drives_.begin(), i->drives_.end(), si->drives_.begin(), si->drives_.end(),
                                       std::inserter(v_union, v_union.begin()));
                        std::set<int> s_union;
                        for (std::vector<int>::const_iterator ui = v_union.begin(); ui != v_union.end(); ++ui)
                            s_union.insert(*ui);
                        i->drives_ = s_union;
                        si->drives_ = s_union;
                        modified = true;
                    }
                }
            }
        }
    }

    int thread_n = 1;
    for (std::vector<DirectoryDesc>::iterator i = directories_to_scan_.begin(); i != directories_to_scan_.end(); ++i)
    {
        if (i->thread_ == 0)
        {
            i->thread_ = thread_n;
            for (std::vector<DirectoryDesc>::iterator si = i + 1; si != directories_to_scan_.end(); ++si)
            {
                if (i->drives_ == si->drives_)
                    si->thread_ = thread_n;
            }
            ++thread_n;
        }
    }
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

    print_windows_error(::GetLastError(), "AdjustTokenPrivileges:");

    return true;
}

struct FileKey
{
    unsigned __int64 size_;
    md5_hash hash_;

    bool operator < (const FileKey& other) const
    {
        if (size_ < other.size_)
            return true;
        else if (size_ > other.size_)
            return false;

        return memcmp(hash_.hash, other.hash_.hash, sizeof(hash_.hash)) < 0;
    }
};

struct FileDesc
{
    FileDesc(const std::string& name = std::string(), const std::string& path = std::string()) :
        name_(name),
        path_(path)
    {}

    std::string name_;
    std::string path_;
};

class FsIterator
{
public:
    FsIterator(const std::vector<tstring>& directories, int seq);

    int get_seq() const;
    void iterate_dirs();
    void calculate_hashes();

    typedef std::multimap<FileKey, FileDesc> Files;

    static void print_results(const Files& result);
    void merge_maps(Files& result) const;
private:
    void iterate_dir(const TCHAR* cname);
    void add_file(__int64 size, const std::string& name, const std::string& path, const md5_hash& hash);
    bool calculate_hash(__int64& size, const std::string& path, md5_hash& hash) const;
    
    std::vector<tstring> directories_;
    int seq_;
    Files files_;
    static const size_t BUFFER_SIZE = 1024*256;
    mutable std::vector<char> buffer_;
};

FsIterator::FsIterator(const std::vector<tstring>& directories, int seq):
    directories_(directories),
    seq_(seq)
{
    buffer_.resize(BUFFER_SIZE);
}

int FsIterator::get_seq() const
{
    return seq_;
}

bool FsIterator::calculate_hash(__int64& size, const std::string& path, md5_hash& hash) const
{
    const tstring wpath = string_utils::to_tstring(path.c_str(), CP_UTF8);

    HANDLE file = ::CreateFile(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
                               OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        const DWORD error = ::GetLastError();
        const std::string msg = "file \"" + path + "\" could not be open:";
        print_windows_error(error, msg.c_str());
        return false;
    }

    std::unique_ptr<HANDLE, std::function<void (HANDLE*)> > close_guard(&file, [](HANDLE* h){if (*h != INVALID_HANDLE_VALUE) ::CloseHandle(*h);});

    __int64 total_bytes = 0;

    md5_state_t state;
    md5_init(&state);

    for (;;)
    {
        DWORD read_bytes = 0;

        if (!::ReadFile(file, &buffer_[0], static_cast<DWORD>(buffer_.size()), &read_bytes, NULL))
        {
            const DWORD error = ::GetLastError();
            const std::string msg = "unable to read from \"" + path + "\" :";
            print_windows_error(error, msg.c_str());
            break;
        }
        if (read_bytes == 0)
            break;

        md5_append(&state, reinterpret_cast<const BYTE*>(&buffer_[0]), static_cast<size_t>(read_bytes));

        total_bytes += read_bytes;
    }

    md5_finish(&state, hash.hash);

    if (size != total_bytes)
    {
        std::cerr << "Warning: " << total_bytes << " read from file \"" << path << "\" with initial size " << size << std::endl;
        size = total_bytes;
    }
    if (total_bytes == 0)
    {
        static const md5_hash zero_hash = {0,};
        hash = zero_hash;
    }
    return true;
}

void FsIterator::add_file(__int64 size, const std::string& name, const std::string& path, const md5_hash& hash)
{
    const FileKey key = {size, hash};
    FileDesc fd(name, path);

    files_.insert(std::pair<FileKey, FileDesc>(key, fd));
}

void FsIterator::print_results(const Files& result)
{
    for (std::multimap<FileKey, FileDesc>::const_iterator mmci = result.begin(); mmci != result.end(); ++mmci)
    {
        static const size_t hash_size = sizeof(mmci->first.hash_.hash)/sizeof(mmci->first.hash_.hash[0]);
        char hash[hash_size*2 + 1];
        for (int i = 0; i < hash_size; ++i)
            _snprintf_s(hash + i*2, sizeof(hash) - i*2, sizeof(hash) - 1, "%02X", mmci->first.hash_.hash[i]);

        std::cout << mmci->first.size_ << " " << hash << " ";
        std::cout << "\"" << mmci->second.name_ << "\" \"" << mmci->second.path_ << "\"" << std::endl;
    }
}

void FsIterator::iterate_dir(const TCHAR* cname)
{
    WIN32_FIND_DATA data;
    int next_res = 1;

    tstring name(cname);
    name = name + _T("\\*");

    HANDLE hFind = ::FindFirstFile(name.c_str(), &data);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        tstring msg(_T("FindFirstFile: "));
        msg += name + _T(" -");
        print_windows_error(::GetLastError(), string_utils::to_ansi_string(msg.c_str(), CP_UTF8).c_str());
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

                static const md5_hash zero_hash = {0,};
                add_file(size, name, path, zero_hash);
            }
        }
    }
}

void FsIterator::iterate_dirs()
{
    for (std::vector<tstring>::const_iterator i = directories_.begin(); i != directories_.end(); ++i)
        iterate_dir(i->c_str());
}

void FsIterator::calculate_hashes()
{
    Files without_signatures;
    without_signatures.swap(files_);

    size_t skipped = 0;

    for (std::multimap<FileKey, FileDesc>::const_iterator i = without_signatures.begin(); i != without_signatures.end(); ++i)
    {
        md5_hash hash = {0,};

        __int64 size = i->first.size_;
        if (!calculate_hash(size, i->second.path_, hash))
            ++skipped;

        add_file(size, i->second.name_, i->second.path_, hash);
    }

    size_t without_sig_cnt = 0;
    for (Files::iterator i = without_signatures.begin(); i != without_signatures.end(); ++i, ++without_sig_cnt) {}

    size_t with_sig_cnt = 0;
    for (Files::iterator i = files_.begin(); i != files_.end(); ++i, ++with_sig_cnt) {}

    std::cerr << "thread " << seq_ << ": without sig " << without_sig_cnt << ", with sig " << with_sig_cnt << ", skipped " << skipped << std::endl;
}

void FsIterator::merge_maps(Files& result) const
{
    for (Files::const_iterator i = files_.begin(); i != files_.end(); ++i)
        result.insert(std::pair<FileKey, FileDesc>(i->first, i->second));
}

static
unsigned __stdcall worker(void* vfsi)
{
    FsIterator* fsi = reinterpret_cast<FsIterator*>(vfsi);

    fsi->iterate_dirs();
    std::cerr << "thread " << fsi->get_seq() << ": first step done" << std::endl;

    fsi->calculate_hashes();

    return 0;
}

class FileMerger
{
public:
    FileMerger() {}
    ~FileMerger() {}

    int merge_data(int argc, TCHAR* argv[]);

private:
    void process_file(TCHAR* name);

    FsIterator::Files files_;
};

int FileMerger::merge_data(int argc, TCHAR* argv[])
{
    if (argc <= 2)
        return 0;

    for (int i = 1; i < argc; ++i)
        process_file(argv[i]);

    FsIterator::print_results(files_);

    return 0;
}

void FileMerger::process_file(TCHAR* name)
{
    std::ifstream ifile;
    ifile.open(name);

    if (!ifile.is_open())
    {
        std::wcerr << _T("file \"") << name << _T("\" could not be open") << std::endl;
        return;
    }

    std::string cname = string_utils::to_ansi_string(name, CP_UTF8);
    size_t pos = cname.rfind(".txt");
    if (pos != std::string::npos)
        cname = cname.substr(0, pos);

    std::cerr << "processing \"" << cname << "\"" << std::endl;

    while (!ifile.eof())
    {
        std::string str;
        std::getline(ifile, str);

        if (str.length() == 0)
            continue;

        std::regex regex("\"([^\"]*)\"|(\\S+)");
        std::sregex_iterator begin = std::sregex_iterator(str.begin(), str.end(), regex);
        std::sregex_iterator end = std::sregex_iterator();

        std::vector<std::string> file_properties;

        for (std::sregex_iterator i = begin; i != end; ++i) {
            std::smatch match = *i;
            const std::string match_str = match.str();
            file_properties.push_back(match_str);
//            std::cout << match_str << '\n';
        }

        if (file_properties.size() != 4)
            std::cerr << "potentially wrong line \"" << str << "\"" << std::endl;
        else
        {
            unsigned __int64 size = 0;
            if (sscanf_s(file_properties[0].c_str(), "%llu", &size) != 1)
                std::cerr << "unable to scan size from \"" << str << "\n" << std::endl;

            md5_hash hash;
//            memset(hash.hash, '0', sizeof(hash.hash));
            size_t hash_size = sizeof(hash.hash) / sizeof(hash.hash[0]);
            if (file_properties[1].length() != hash_size*2)
                std::cerr << "unable to scan hash (invalid length) from \"" << str << "\n" << std::endl;
            else
            {
                for (size_t i = 0; i < hash_size; ++i)
                {
                    const std::string sb = file_properties[1].substr(i*2, 2);
                    int b = 0;
                    if (sscanf_s(sb.c_str(), "%02X", &b) != 1)
                        std::cerr << "unable to scan hash (invalid bytes) from \"" << str << "\n" << std::endl;

                    hash.hash[i] = b;
                }
            }

            std::string name = file_properties[2];
            if (name.length() > 0 && name[0] == '"')
                name = name.substr(1);
            if (name.length() > 0 && name[name.length() - 1] == '"')
                name = name.substr(0, name.length() - 1);

            std::string path = file_properties[3];
            if (path.length() > 0 && path[0] == '"')
                path = path.substr(1);
            if (path.length() > 0 && path[path.length() - 1] == '"')
                path = path.substr(0, path.length() - 1);

            if (path.substr(0, 4) == "\\\\?\\")
                path = "\\\\" + cname + path.substr(3);

            const FileKey key = {size, hash};
            const FileDesc desc(name, path);
            files_.insert(std::pair<FileKey, FileDesc>(key, desc));
        }
    }
}

int _tmain(int argc, _TCHAR* argv[])
{
    if (argc < 2)
    {
        std::wcerr << _T("please specify configuration file name or names of files to merge") << std::endl;
        return 1;
    }

    if (argc > 2)
    {
        FileMerger fm;
        return fm.merge_data(argc, argv);
    }

    obtain_backup_priviledge();

    FileConfig fc(argv[1]);
    fc.initialize();

    std::vector<FsIterator> vec_fsi;

    for (int thread_n = 1; ; ++thread_n)
    {
        const std::vector<tstring> dirs = fc.get_directories(thread_n);
        if (dirs.empty())
            break;
        std::wcerr << thread_n << _T(":");
        for (std::vector<tstring>::const_iterator ci = dirs.begin(); ci != dirs.end(); ++ci)
            std::wcerr << _T(" ") << ci->c_str();

        std::wcerr << std::endl;
        FsIterator fsi(dirs, thread_n);
        vec_fsi.push_back(fsi);
    }

    size_t thrd_num = vec_fsi.size();
    std::vector<HANDLE> threads;
    threads.resize(thrd_num);

    for (size_t i = 0; i < thrd_num; ++i)
    {
        threads[i] = reinterpret_cast<HANDLE>(_beginthreadex(NULL, 0, worker, &vec_fsi[i], 0, NULL));
        if (threads[i] == NULL)
        {
            std::cerr << "Unable to start thread" << std::endl;
            return 1;
        }
    }

    ::WaitForMultipleObjects(static_cast<DWORD>(thrd_num), &threads[0], true, INFINITE);

    FsIterator::Files result;
    for (std::vector<FsIterator>::const_iterator i = vec_fsi.begin(); i != vec_fsi.end(); ++i)
        i->merge_maps(result);

    FsIterator::print_results(result);

    return 0;
}
