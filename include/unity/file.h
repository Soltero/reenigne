#ifndef INCLUDED_FILE_H
#define INCLUDED_FILE_H

template<class T> class CurrentDirectoryTemplate;
typedef CurrentDirectoryTemplate<void> CurrentDirectory;

template<class T> class FileSystemObjectTemplate;
typedef FileSystemObjectTemplate<void> FileSystemObject;

template<class T> class FindHandleTemplate;
typedef FindHandleTemplate<void> FindHandle;

template<class T> class DirectoryTemplate;
typedef DirectoryTemplate<void> Directory;

template<class T> class FileTemplate;
typedef FileTemplate<void> File;

template<class T> class RootDirectoryImplementationTemplate;
typedef RootDirectoryImplementationTemplate<void> RootDirectoryImplementation;

template<class T> class RootDirectoryTemplate;
typedef RootDirectoryTemplate<void> RootDirectory;

template<class T> class NamedFileSystemObjectImplementationTemplate;
typedef NamedFileSystemObjectImplementationTemplate<void> NamedFileSystemObjectImplementation;

#ifdef _WIN32
template<class T> class DriveRootDirectoryTemplate;
typedef DriveRootDirectoryTemplate<void> DriveRootDirectory;

template<class T> class UNCRootDirectoryTemplate;
typedef UNCRootDirectoryTemplate<void> UNCRootDirectory;

template<class T> class DriveCurrentDirectoryTemplate;
typedef DriveCurrentDirectoryTemplate<void> DriveCurrentDirectory;
#endif

#include "unity/string.h"
#include "unity/character_source.h"

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif

template<class T> class FileSystemObjectTemplate
{
public:
    FileSystemObjectTemplate(const String& path, const Directory& relativeTo = CurrentDirectory(), bool windowsParsing = false)
    {
        *this = FileSystemObject::parse(path, relativeTo, windowsParsing);
    }

    DirectoryTemplate<T> parent() const { return _implementation->parent(); }
    String name() const { return _implementation->name(); }
    bool isRoot() const { return _implementation->isRoot(); }
    String path() const { return _implementation->path(); }

    bool operator==(const FileSystemObject& other) const
    {
        return _implementation->compare(other._implementation) == 0;
    }
    bool operator!=(const FileSystemObject& other) const { return !operator==(other); }
    int hash() const { return _implementation->hash(0); }
#ifdef _WIN32
    String windowsPath() const { return _implementation->windowsPath(); }
#endif
    class Implementation : public ReferenceCounted
    {
    public:
        virtual Directory parent() const = 0;
        virtual String name() const = 0;
    #ifdef _WIN32
        virtual String windowsPath() const = 0;
    #endif
        virtual String path() const = 0;
        virtual bool isRoot() const = 0;
        virtual int hash(int h) const = 0;
        virtual int compare(const Implementation* other) const = 0;
    };

protected:
    FileSystemObjectTemplate(Reference<Implementation> implementation) : _implementation(implementation) { }

    Reference<Implementation> _implementation;
private:

    static FileSystemObject parse(const String& path, const Directory& relativeTo, bool windowsParsing)
    {
        if (path.empty()) {
            static String invalidPath("Invalid path");
            throw Exception(invalidPath);
        }

#ifdef _WIN32
        if (windowsParsing)
            return windowsParse(path, relativeTo);
#endif
        return parse(path, relativeTo);
    }

#ifdef _WIN32
    static DirectoryTemplate<T> windowsParseRoot(const String& path, const Directory& relativeTo, CodePointSource& s)
    {
        static String invalidPath("Invalid path");

        CodePointSource s2 = s;
        int c = s2.get();
        Directory dir = relativeTo;

        // Process initial slashes
        if (c == '/' || c == '\\') {
            s = s2;
            dir = RootDirectory();
            c = s2.get();
            if (c == -1)
                return dir;
            if (c == '/' || c == '\\') {
                int serverStart = s2.offset();
                int p;
                do {
                    p = s2.offset();
                    c = s2.get();
                    if (c == -1)
                        throw Exception(invalidPath);
                    // TODO: What characters are actually legal in server names?
                } while (c != '\\' && c != '/');
                String server = s2.subString(serverStart, p);
                int shareStart;
                do {
                    shareStart = s2.offset();
                    c = s2.get();
                } while (c == '/' || c == '\\');
                do {
                    p = s2.offset();
                    c = s2.get();
                    // TODO: What characters are actually legal in share names?
                } while (c != '\\' && c != '/' && c != -1);
                String share = s2.subString(shareStart, p);
                dir = UNCRootDirectory(server, share);
                do {
                    s = s2;
                    c = s2.get();
                } while (c == '/' || c == '\\');
            }
            // TODO: In paths starting \\?\, only \ and \\ are allowed separators, and ?*:"<> are allowed.
            // see http://docs.racket-lang.org/reference/windowspaths.html for more details
            return dir;
        }
        int drive = (c >= 'a' ? (c - 'a') : (c - 'A'));
        if (drive < 0 || drive >= 26)
            return dir;
        c = s2.get();
        if (c != ':')
            return dir;
        s = s2;
        c = s2.get();
        if (c == '/' || c == '\\') {
            dir = DriveRootDirectory(drive);
            while (c == '/' || c == '\\') {
                s = s2;
                c = s2.get();
            }
            return dir;
        }
        return DriveCurrentDirectory(drive);
    }

    static FileSystemObject windowsParse(const String& path, const Directory& relativeTo)
    {
        static String invalidPath("Invalid path");
        static String currentDirectory(".");
        static String parentDirectory("..");
        static String empty;

        CodePointSource s(path);
        Directory dir = windowsParseRoot(path, relativeTo, s);
        int subDirectoryStart = s.offset();
        int c = s.get();

        String name;
        do {
            int p;
            while (c != '/' && c != '\\') {
                if (c < 32 || c == '?' || c == '*' || c == ':' || c == '"' || c == '<' || c == '>')
                    throw Exception(invalidPath);
                p = s.offset();
                c = s.get();
                if (c == -1)
                    break;
            }
            name = s.subString(subDirectoryStart, p);
            if (name == currentDirectory)
                name = empty;
            if (name == parentDirectory) {
                dir = dir.parent();
                name = empty;
            }
            if (name != empty) {
                int l = name[name.length() - 1];
                if (l == '.' || l == ' ')
                    throw Exception(invalidPath);
            }
            if (c == -1)
                break;
            while (c == '/' || c == '\\') {
                subDirectoryStart = s.offset();
                c = s.get();
                if (c == -1)
                    break;
            }
            if (c == -1)
                break;
            if (name != empty)
                dir = dir.subDirectory(name);
        } while (true);
        if (name == empty) {
            if (dir.isRoot())
                return dir;
            return FileSystemObject(dir.parent(), dir.name());
        }
        return FileSystemObject(dir, name);
    }
#endif

    static DirectoryTemplate<T> parseRoot(const String& path, const Directory& relativeTo, CodePointSource& s)
    {
        CodePointSource s2 = s;
        int c = s.get();
        Directory dir = relativeTo;

        // Process initial slashes
        if (c == '/') {
            dir = RootDirectory();
            while (c == '/') {
                s = s2;
                c = s2.get();
            }
        }
        return dir;
    }

    static FileSystemObject parse(const String& path, const Directory& relativeTo)
    {
        static String currentDirectory(".");
        static String parentDirectory("..");
        static String empty;

        CodePointSource s(path);
        Directory dir = parseRoot(path, relativeTo, s);
        int subDirectoryStart = s.offset();
        int c = s.get();

        String name;
        do {
            int p;
            while (c != '/') {
                if (c == 0) {
                    static String invalidPath("Invalid path");
                    throw Exception(invalidPath);
                }
                p = s.offset();
                c = s.get();
                if (c == -1)
                    break;
            }
            name = s.subString(subDirectoryStart, p);
            if (name == currentDirectory)
                name = empty;
            if (name == parentDirectory) {
                dir = dir.parent();
                name = empty;
            }
            if (c == -1)
                break;
            while (c == '/') {
                subDirectoryStart = s.offset();
                c = s.get();
                if (c == -1)
                    break;
            }
            if (c == -1)
                break;
            if (name != empty)
                dir = dir.subDirectory(name);
        } while (true);
        if (name == empty) {
            if (dir.isRoot())
                return dir;
            return FileSystemObject(dir.parent(), dir.name());
        }
        return FileSystemObject(dir, name);
    }

    FileSystemObjectTemplate(const Directory& parent, const String& name) : _implementation(new NamedFileSystemObjectImplementation(parent, name)) { }

    template<class T> friend class NamedFileSystemObjectImplementationTemplate;
    template<class T> friend class CurrentDirectoryTemplate;
    template<class T> friend class DirectoryTemplate;

    template<class T> friend void applyToWildcard(T functor, const String& wildcard, int recurseIntoDirectories, const Directory& relativeTo);
};

#ifdef _WIN32
template<class T> class FindHandleTemplate
{
public:
    FindHandleTemplate(const Directory& directory, const String& wildcard)
      : _directory(directory), _wildcard(wildcard), _complete(false),
        _handle(INVALID_HANDLE_VALUE)
    {
        Array<WCHAR> data;
        _path = directory.child(wildcard).windowsPath();
        _path.copyToUTF16(&data);
        _handle = FindFirstFile(&data[0], &_data);
        if (_handle == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_FILE_NOT_FOUND)
                _complete = true;
            else
                throwError();
        }
        String n = name();
        static String currentDirectory(".");
        static String parentDirectory("..");
        if (n == currentDirectory || n == parentDirectory)
            next();
    }
    void next()
    {
        do {
            if (FindNextFile(_handle, &_data) == 0)
                if (GetLastError() == ERROR_NO_MORE_FILES) {
                    _complete = true;
                    return;
                }
                else
                    throwError();
            String n = name();
            static String currentDirectory(".");
            static String parentDirectory("..");
            if (n == currentDirectory || n == parentDirectory)
                continue;
            break;
        } while (true);
    }
    ~FindHandleTemplate()
    {
        if (_handle != INVALID_HANDLE_VALUE)
            FindClose(_handle);
    }
    bool isDirectory() const
    {
        return (_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    String name() const
    {
        return _data.cFileName;
    }
    FileSystemObject object() const
    {
        return _directory.child(name());
    }
    DirectoryTemplate<T> directory() const
    {
        return _directory.subDirectory(name());
    }
    FileTemplate<T> file() const
    {
        return _directory.file(name());
    }
    bool complete() { return _complete; }
private:
    void throwError()
    {
        static String findingFiles("Finding files ");
        Exception::throwSystemError(findingFiles + _path);
    }

    HANDLE _handle;
    WIN32_FIND_DATA _data;
    Directory _directory;
    String _wildcard;
    String _path;
    bool _complete;
};
#else
template<class T> class FindHandleTemplate
{
public:
    FindHandleTemplate(const Directory& directory, const String& wildcard)
      : _directory(directory), _wildcard(wildcard), _complete(false),
        _dir(NULL)
    {
        Array<UInt8> data;
        _path = directory.path();
        _path.copyTo(&data);
        _dir = opendir(reinterpret_cast<const char*>(&data[0]));
        if (_dir == NULL) {
            static String openingFile("Opening directory ");
            Exception::throwSystemError(openingDirectory + _path);
        }
        next();
    }
    void next()
    {
        do {
            errno = 0;
            _data = readdir(_dir);
            if (_data == NULL)
                if (errno == 0)
                    _complete = true;
                else {
                    static String openingFile("Reading directory ");
                    Exception::throwSystemError(openingDirectory + _path);
                }
            String n = name();
            static String currentDirectory(".");
            static String parentDirectory("..");
            if (n == currentDirectory || n == parentDirectory)
                continue;
            if (!matches(n, wildcard))
                continue;
            break;
        } while (true);
    }
    ~FindHandle()
    {
        if (_handle != NULL)
            closedir(_dir);
    }
    bool isDirectory() const
    {
        return _dirent->d_type == DT_DIR;
    }
    String name() const
    {
        return _data.d_name;
    }
    FileSystemObject object() const
    {
        return _directory.child(name());
    }
    Directory directory() const
    {
        return _directory.subDirectory(name());
    }
    File file() const
    {
        return _directory.file(name());
    }
    bool complete() { return _complete; }
private:
    static bool matches(String name, String wildcard)
    {
        CodePointSource sw(wildcard);
        CodePointSource sn(name);
        do {
            int cs = sw.get();
            int cn = sn.get();
            switch (cs) {
                case '?':
                    if (cn == -1)
                        return false;
                    continue;
                case '*':
                    // TODO: this code is O(n^p) where p is number of stars, we can do better using dynamic programming.
                    if (cn == -1)
                        continue;
                    do {
                        if (matches(name.subString(sn.offset(), name.length() - sn.offset()), sw.offset(), wildcard.length() - sw.offset()))
                            return true;
                        cn = sn.get();
                    } while (cn != -1);
                    return false;
                case -1:
                    return (cn == -1);
                default:
                    return (cn == cs);
            }
        } while (true);
    }

    struct dirent* _data;
    DIR* _dir;
    String _path;
    bool _complete;
};
#endif

template<class T> class DirectoryTemplate : public FileSystemObject
{
public:
    DirectoryTemplate(const String& path, const Directory& relativeTo = CurrentDirectory(), bool windowsParsing = false) : FileSystemObject(path, relativeTo, windowsParsing) { }
    FileSystemObject child(const String& name) const
    {
        return FileSystemObject(*this, name);
    }
    Directory subDirectory(const String& subDirectoryName) const
    {
        return Directory(child(subDirectoryName));
    }
    FileTemplate<T> file(const String& fileName) const
    {
        return File(child(fileName));
    }
    template<class F> void applyToContents(F functor, bool recursive, const String& wildcard = String("*")) const
    {
        FindHandle handle(*this, wildcard);
        while (!handle.complete()) {
            if (handle.isDirectory()) {
                Directory child = handle.directory();
                if (recursive)
                    child.applyToContents(functor, true);
                else
                    functor(child);
            }
            else
                functor(handle.file());
            handle.next();
        }
    }
protected:
    DirectoryTemplate(FileSystemObject object) : FileSystemObject(object) { }
    DirectoryTemplate(Reference<FileSystemObject::Implementation> implementation) : FileSystemObject(implementation) { }
};

template<class T> class CurrentDirectoryTemplate : public Directory
{
public:
    CurrentDirectoryTemplate() : Directory(implementation()) { }

private:
    static Reference<FileSystemObject::Implementation> _implementation;
    static Reference<FileSystemObject::Implementation> implementation()
    {
        if (!_implementation.valid())
            _implementation = currentDirectory();
        return _implementation;
    }

    static Reference<FileSystemObject::Implementation> currentDirectory()
    {
        static String obtainingCurrentDirectory("Obtaining current directory");
#ifdef _WIN32
        int n = GetCurrentDirectory(0, NULL);
        if (n == 0)
            Exception::throwSystemError(obtainingCurrentDirectory);
        Array<WCHAR> buf;
        buf.allocate(n);
        if (GetCurrentDirectory(n, &buf[0]) == 0)
            Exception::throwSystemError(obtainingCurrentDirectory);
        String path(&buf[0]);
        return FileSystemObject::parse(path, RootDirectory(), true)._implementation;
#else
        size_t size = 100;
        do {
            Array<char> buf(size);
            if (getcwd(&buf[0], size) != 0) {
                String path(&buf[0]);
                return FileSystemObject::parse(path, RootDirectory(), false)._implementation;
            }
            if (errno != ERANGE)
                Exception::throwSystemError(obtainingCurrentDirectory);
            size *= 2;
        } while (true);
#endif
    }

#ifdef _WIN32
    template<class T> friend class DriveCurrentDirectoryTemplate;
#endif
};

template<class T> Reference<FileSystemObject::Implementation> CurrentDirectoryTemplate<T>::_implementation;

#ifdef _WIN32
template<class T> class DriveCurrentDirectoryTemplate : public Directory
{
public:
    DriveCurrentDirectoryTemplate(int drive) : Directory(implementation(drive)) { }
private:
    static Reference<FileSystemObject::Implementation> _implementations[26];
    static Reference<FileSystemObject::Implementation> implementation(int drive)
    {
        if (!_implementations[drive].valid()) {
            static String settingCurrentDirectory("Setting current directory");
            static String obtainingCurrentDirectory("Obtaining current directory");  // TODO: can this be shared with the copy in CurrentDirectoryImplementation?

            // Make sure the current directory has been retrieved
            CurrentDirectory();

            // Change to this drive
            WCHAR buf[3];
            buf[0] = drive + 'A';
            buf[1] = ':';
            buf[2] = 0;
            if (SetCurrentDirectory(&buf[0]) == 0)
                Exception::throwSystemError(settingCurrentDirectory);

            // Retrieve current directory
            _implementations[drive] = CurrentDirectory::currentDirectory();
        }
        return _implementations[drive];
    }
};

template<class T> Reference<FileSystemObject::Implementation> DriveCurrentDirectoryTemplate<T>::_implementations[26];

#endif

template<class T> class RootDirectoryTemplate : public Directory
{
public:
    RootDirectoryTemplate() : Directory(implementation()) { }

    class Implementation : public FileSystemObject::Implementation
    {
    public:
        Implementation() { }

        Directory parent() const { return RootDirectory(); }
        String name() const
        {
            static String empty;
            return empty;
        }
    #ifdef _WIN32
        String windowsPath() const
        {
            // TODO: Use \\?\ to avoid MAX_PATH limit?
            // If we do this we need to know the current drive - this is the first character of CurrentDirectory().windowsPath() .
            static String backslash("\\");
            return backslash;
        }
    #endif
        String path() const
        {
            static String empty("");
            return empty;
        }
        bool isRoot() const { return true; }

        int hash(int h) const { return 0; }

        int compare(const FileSystemObject::Implementation* other) const
        {
            const Implementation* root = dynamic_cast<const Implementation*>(other);
            if (root == 0)
                return 1;
            return 0;
        }
    };
private:
    static Reference<Implementation> _implementation;
    static Reference<Implementation> implementation()
    {
        if (!_implementation.valid())
            _implementation = new Implementation();
        return _implementation;
    }
};


template<class T> Reference<RootDirectory::Implementation> RootDirectoryTemplate<T>::_implementation;

#ifdef _WIN32
template<class T> class DriveRootDirectoryTemplate : public Directory
{
public:
    DriveRootDirectoryTemplate(int drive) : Directory(implementation(drive)) { }
private:
    static Reference<FileSystemObject::Implementation> _implementations[26];
    static Reference<FileSystemObject::Implementation> implementation(int drive)
    {
        if (!_implementations[drive].valid())
            _implementations[drive] = new Implementation(drive);
        return _implementations[drive];
    }
    class Implementation : public RootDirectory::Implementation
    {
    public:
        Implementation(int drive) : _drive(drive) { }

        Directory parent() const { return DriveRootDirectory(_drive); }
        String windowsPath() const
        {
            // TODO: Use \\?\ to avoid MAX_PATH limit?
            Reference<OwningBufferImplementation> bufferImplementation = new OwningBufferImplementation();
            bufferImplementation->allocate(2);
            UInt8* p = bufferImplementation->data();
            p[0] = _drive + 'A';
            p[1] = ':';
            return String(Buffer(bufferImplementation), 0, 2);
        }
        String path() const
        {
            static String colon(":");
            return String::codePoint('A' + _drive) + colon;
        }

        int hash(int h) const { return _drive; }

        int compare(const FileSystemObject::Implementation* other) const
        {
            const Implementation* root = dynamic_cast<const Implementation*>(other);
            if (root == 0)
                return 1;
            if (_drive != root->_drive)
                return 1;
            return 0;
        }
    private:
        int _drive;
    };
};

template<class T> Reference<FileSystemObject::Implementation> DriveRootDirectoryTemplate<T>::_implementations[26];

template<class T> class UNCRootDirectoryTemplate : public Directory
{
public:
    UNCRootDirectoryTemplate(const String& server, const String& share) : Directory(new Implementation(server, share)) { }
private:
    class Implementation : public RootDirectory::Implementation
    {
    public:
        Implementation(const String& server, const String& share) : _server(server), _share(share) { }

        Directory parent() const { return UNCRootDirectory(_server, _share); }
        String windowsPath() const
        {
            static String backslashBackslash("\\\\");
            static String backslash("\\");
            return backslashBackslash + _server + backslash + _share;
        }

        int hash(int h) const { return (h*67 + _server.hash())*67 + _share.hash(); }

        int compare(const FileSystemObject::Implementation* other) const
        {
            const Implementation* root = dynamic_cast<const Implementation*>(other);
            if (root == 0)
                return 1;
            if (_server != root->_server)
                return 1;
            if (_share != root->_share)
                return 1;
            return 0;
        }
    private:
        String _server;
        String _share;
    };
};
#endif

class FileHandle : public AutoHandle
{
public:
    void openRead(const File& file)
    {
#ifdef _WIN32
        Array<WCHAR> data;
        String path = file.windowsPath();
        path.copyToUTF16(&data);
        HANDLE h = CreateFile(
           &data[0],
           GENERIC_READ,
           FILE_SHARE_READ,
           NULL,
           OPEN_EXISTING,
           FILE_FLAG_SEQUENTIAL_SCAN,
           NULL);
        if (h == INVALID_HANDLE_VALUE) {
            static String openingFile("Opening file ");
            throw Exception::systemError(openingFile + path);
        }
        set(h, path);
#else
        Array<UInt8> data;
        String path = file.path();
        path.copyTo(&data);
        int fileDescriptor = open(
            reinterpret_cast<const char*>(&data[0]),
            O_RDONLY);
        if (fileDescriptor == -1) {
            static String openingFile("Opening file ");
            throw Exception::systemError(openingFile + path);
        }
        set(fileDescriptor, path);
#endif
    }
    void openWrite(const File& file)
    {
#ifdef _WIN32
        Array<WCHAR> data;
        String path = file.windowsPath();
        path.copyToUTF16(&data);
        HANDLE h = CreateFile(
            &data[0],
            GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (h == INVALID_HANDLE_VALUE) {
            static String openingFile("Opening file ");
            Exception::throwSystemError(openingFile + path);
        }
        set(h, path);
#else
        Array<UInt8> data;
        String path = file.path();
        path.copyTo(&data);
        int fileDescriptor = open(
            reinterpret_cast<const char*>(&data[0]),
            O_WRONLY | O_CREAT | O_TRUNC);
        if (fileDescriptor == -1) {
            static String openingFile("Opening file ");
            Exception::throwSystemError(openingFile + path);
        }
        set(fileDescriptor, path);
#endif
    }
};

template<class T> class FileTemplate : public FileSystemObject
{
public:
    FileTemplate(const String& path, const Directory& relativeTo = CurrentDirectory(), bool windowsParsing = false) : FileSystemObject(path, relativeTo, windowsParsing) { }

    String contents() const
    {
        FileHandle handle;
        handle.openRead(*this);
#ifdef _WIN32
        LARGE_INTEGER size;
        if (GetFileSizeEx(handle, &size) == 0) {
            static String obtainingLengthOfFile("Obtaining length of file ");
            Exception::throwSystemError(obtainingLengthOfFile + filePath);
        }
        int n = size.LowPart;
        if (size.HighPart != 0 || n >= 0x80000000) {
            static String tooLargeFile("2Gb or more in file ");
            Exception::throwSystemError(tooLargeFile + filePath);
        }
        Reference<OwningBufferImplementation> bufferImplementation = new OwningBufferImplementation();
        bufferImplementation->allocate(n);
        DWORD numberOfBytesRead;
        if (ReadFile(handle, static_cast<LPVOID>(bufferImplementation->data()), n, &numberOfBytesRead, NULL) == 0 || numberOfBytesRead != n) {
            static String readingFile("Reading file ");
            Exception::throwSystemError(readingFile + filePath);
        }
        return String(Buffer(bufferImplementation), 0, n);
#else
        off_t n = lseek(fileDescriptor, 0, SEEK_END);
        static String seekingFile("Seeking file ");
        if (n == (off_t)(-1))
            Exception::throwSystemError(seekingFile + filePath);
        if (n >= 0x80000000) {
            static String tooLargeFile("2Gb or more in file ");
            Exception::throwSystemError(tooLargeFile + filePath);
        }
        off_t r = lseek(fileDescriptor, 0, SEEK_SET);
        if (r == (off_t)(-1))
            Exception::throwSystemError(seekingFile + filePath);
        Reference<OwningBufferImplementation> bufferImplementation = new OwningBufferImplementation();
        bufferImplementation->allocate(n);
        ssize_t readResult = read(fileDescriptor, static_cast<void*>(bufferImplementation->data()), n);
        if (readResult < n) {
            static String readingFile("Reading file ");
            Exception::throwSystemError(readingFile + filePath);
        }
        return String(Buffer(bufferImplementation), 0, n);
#endif
    }
    void save(const String& contents)
    {
        FileHandle handle;
        handle.openWrite(*this);
        contents.write(handle);
    }
    void secureSave(const String& contents)
    {
        // TODO: Backup file?
#ifdef _WIN32
        String filePath = windowsPath();
        Array<WCHAR> data;
        filePath.copyToUTF16(&data);
        Array<WCHAR> tempData;
        int i = 0;
        do {
            String tempPath = filePath + String::hexadecimal(i, 8);
            tempPath.copyToUTF16(&tempData);
            HANDLE h = CreateFile(
                &tempData[0],
                GENERIC_WRITE,
                0,
                NULL,
                CREATE_NEW,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                NULL);
            if (h != INVALID_HANDLE_VALUE)
                break;
            if (GetLastError() != ERROR_FILE_EXISTS) {
                static String openingFile("Opening file ");
                Exception::throwSystemError(openingFile + tempPath);
            }
        } while (true);
        {
            AutoHandle handle(h);
            contents.write(handle);
        }
        if (ReplaceFile(&data[0], &tempData[0], NULL, REPLACEFILE_WRITE_THROUGH | REPLACEFILE_IGNORE_MERGE_ERRORS) == 0) {
            static String replacingFile("Replacing file ");
            Exception::throwSystemError(replacingFile + filePath);
        }
#else
        String filePath = path();
        Array<UInt8> data;
        filePath.copyTo(&data);
        Array<UInt8> tempData;
        int i = 0;
        do {
            String tempPath = filePath + String::hexadecimal(i, 8);
            tempPath.copyTo(tempData);
            int fileDescriptor = open(
                reinterpret_cast<const char*>(&data[0]),
                O_WRONLY | O_CREAT | O_EXCL);
            if (fileDescriptor != -1)
                break;
            if (errno != EEXIST) {
                static String openingFile("Opening file ");
                Exception::throwSystemError(openingFile + tempPath);
            }
        } while (true);
        {
            AutoHandle handle(fileDescriptor);
            contents.write(handle);
            if (fsync(handle) != 0) {
                static String synchronizingFile("Synchronizing file ");
                Exception::throwSystemError(synchronizingFile + filePath);
            }
        }
        if (rename(&tempData[0], &data[0]) != 0) {
            static String replacingFile("Replacing file ");
            Exception::throwSystemError(replacingFile + filePath);
        }
#endif
    }
    void append(const String& contents)
    {
#ifdef _WIN32
        Array<WCHAR> data;
        String filePath = windowsPath();
        filePath.copyToUTF16(&data);
        HANDLE h = CreateFile(
            &data[0],
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (h == INVALID_HANDLE_VALUE) {
            static String openingFile("Opening file ");
            Exception::throwSystemError(openingFile + filePath);
        }
        AutoHandle handle(h);
        contents.write(handle);
#else
        Array<UInt8> data;
        String filePath = path();
        filePath.copyTo(&data);
        int fileDescriptor = open(
            reinterpret_cast<const char*>(&data[0]),
            O_WRONLY | O_APPEND);
        if (fileDescriptor == -1) {
            static String openingFile("Opening file ");
            Exception::throwSystemError(openingFile + filePath);
        }
        AutoHandle handle(fileDescriptor);
        contents.write(handle);
#endif
    }
private:
    FileTemplate(FileSystemObject object) : FileSystemObject(object) { }

    friend class DirectoryTemplate<T>;
};


template<class T> class NamedFileSystemObjectImplementationTemplate : public FileSystemObject::Implementation
{
public:
    NamedFileSystemObjectImplementationTemplate(const Directory& parent, const String& name) : _parent(parent), _name(name) { }
#ifdef _WIN32
    String windowsPath() const
    {
        static String windowsPathSeparator("\\");
        return _parent.windowsPath() + windowsPathSeparator + _name;
    }
#endif
    String path() const
    {
        static String pathSeparator("/");
        return _parent.path() + pathSeparator + _name;
    }

    Directory parent() const { return _parent; }

    String name() const { return _name; }

    bool isRoot() const { return false; }

    int hash(int h) const { return (h*67 + _parent.hash())*67 + _name.hash(); }

    int compare(const FileSystemObject::Implementation* other) const
    {
        const NamedFileSystemObjectImplementation* named = dynamic_cast<const NamedFileSystemObjectImplementation*>(other);
        if (named == 0)
            return 1;
        if (_parent != named->_parent)
            return 1;
        if (_name != named->_name)
            return 1;
        return 0;
    }
private:
    Directory _parent;
    String _name;
};

template<class T> void applyToWildcard(T functor, CodePointSource s, int recurseIntoDirectories, Directory directory)
{
    static String invalidPath("Invalid path");
    static String currentDirectory(".");
    static String parentDirectory("..");
    static String empty;

    int subDirectoryStart = s.offset();
    int c = s.get();
    int p;
#ifdef _WIN32
    while (c != '/' && c != '\\' && c != -1) {
        if (c < 32 || c == ':' || c == '"' || c == '<' || c == '>')
            throw Exception(invalidPath);
        p = s.offset();
        c = s.get();
    }
    String name = s.subString(subDirectoryStart, p);
    CodePointSource s2 = s;
    while (c == '/' || c == '\\') {
        s = s2;
        c = s2.get();
    }
    if (name == currentDirectory) {
        if (c == -1)
            if (recurseIntoDirectories)
                name = String("*");
            else {
                functor(directory);
                return;
            }
        else {
            applyToWildcard(functor, s, recurseIntoDirectories, directory);
            return;
        }
    }
    if (name == parentDirectory) {
        if (c == -1)
            if (recurseIntoDirectories) {
                name = String("*");
                directory = directory.parent();
            }
            else {
                functor(directory.parent());
                return;
            }
        else {
            applyToWildcard(functor, s, recurseIntoDirectories, directory.parent());
            return;
        }
    }
    if (name != empty) {
        int l = name[name.length() - 1];
        if (l == '.' || l == ' ')
            throw Exception(invalidPath);
    }
#else
    while (c != '/' && c != -1) {
        p = s.offset();
        c = s.get();
    }
    String name = s.subString(subDirectoryStart, p);
    while (c == '/')
        c = s.get();
    if (name == currentDirectory) {
        applyToWildcard(functor, s, recurseIntoDirectories, directory);
        return;
    }
    if (name == parentDirectory) {
        applyToWildcard(functor, s, recurseIntoDirectories, directory.parent());
        return;
    }
#endif
    FindHandle handle(directory, name);
    while (!handle.complete()) {
        if (handle.isDirectory()) {
            Directory child = handle.directory();
            if (c == -1)
                if (recurseIntoDirectories)
                    child.applyToContents(functor, true);
                else
                    functor(child);
            else
                applyToWildcard(functor, s, recurseIntoDirectories, child);
        }
        else
            if (c == -1)
                functor(handle.file());
        handle.next();
    }
}

template<class T> void applyToWildcard(T functor, const String& wildcard, int recurseIntoDirectories = true, const Directory& relativeTo = CurrentDirectory())
{
    CodePointSource s(wildcard);
#ifdef _WIN32
    Directory dir = FileSystemObject::windowsParseRoot(wildcard, relativeTo, s);
#else
    Directory dir = FileSystemObject::parse(wildcard, relativeTo, s);
#endif
    applyToWildcard(functor, s, recurseIntoDirectories, dir);
}

#endif // INCLUDED_FILE_H
