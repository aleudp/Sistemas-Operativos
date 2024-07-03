#include <ctime>
#include <iostream>
#include <sstream>
#include <vector>
#include <iterator>
#include <map>
#include <string>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <algorithm>
#include <chrono>

using namespace std;
namespace fs = std::filesystem;

struct Inode {
    string name;
    bool isDirectory;
    string permissions;
    Inode* parent;
    map<string, Inode*> children;
    std::time_t creationTime;

    Inode(string n, bool isDir, string perms, Inode* p)
        : name(n), isDirectory(isDir), permissions(perms), parent(p) {
        creationTime = std::time(nullptr);
    }
};

class FileSystem {
public:
    FileSystem() {
        root = new Inode("/", true, "drwxrwxrwx", nullptr);
        currentDirectory = root;
        rootPath = fs::current_path() / "root";
        if (!fs::exists(rootPath)) {
            fs::create_directory(rootPath);
        }
        mapFileSystem(rootPath, root);
    }

    ~FileSystem() {
        deleteInode(root);
    }

    void touch(string name) {
        fs::path filePath = currentPath() / name;
        if (!fs::exists(filePath)) {
            fs::create_directories(filePath.parent_path());

            int fd = open(filePath.c_str(), O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            if (fd != -1) {
                close(fd);

                Inode* file = new Inode(name, false, "-rw-r--r--", currentDirectory);
                currentDirectory->children[name] = file;
            } else {
                cout << "Error: Failed to create file '" << name << "'." << endl;
            }
        } else {
            cout << "Error: A file named '" << name << "' already exists." << endl;
        }
    }

    void mkdir(string name) {
        fs::path dirPath = currentPath() / name;
        if (!fs::exists(dirPath)) {
            if (currentDirectory->permissions[2] != 'w') {
                cout << "Error: No write permission in the current directory '" << currentDirectory->name << "'." << endl;
                return;
            }

            try {
                fs::create_directories(dirPath);

                Inode* directory = new Inode(name, true, "drwxr-xr-x", currentDirectory);
                currentDirectory->children[name] = directory;
            } catch (const fs::filesystem_error& e) {
                cout << "Error: " << e.what() << endl;
            }
        } else {
            cout << "Error: A directory named '" << name << "' already exists." << endl;
        }
    }

    void cd(string name) {
        if (name == "..") {
            if (currentDirectory != root) {
                currentDirectory = currentDirectory->parent;
            }
        } else if (currentDirectory->children.count(name)) {
            Inode* directory = currentDirectory->children[name];
            if (directory->isDirectory) {
                if (directory->permissions[3] != 'x') {
                    cout << "Error: No execute permission for directory '" << name << "'." << endl;
                    return;
                }
                currentDirectory = directory;
            } else {
                cout << "Error: '" << name << "' is a file, not a directory." << endl;
            }
        } else {
            cout << "Error: Directory '" << name << "' not found." << endl;
        }
    }

    void ls() {
        for (const auto& entry : currentDirectory->children) {
            cout << entry.first << endl;
        }
    }

    void ls_l() {
    for (const auto& entry : currentDirectory->children) {
        std::time_t time = entry.second->creationTime;
        struct tm * timeinfo;
        timeinfo = localtime(&time);
        char buffer [80];
        strftime(buffer, 80, "%b %e %R", timeinfo);
        cout << entry.second->permissions << "  "
             << buffer << "  "
             << entry.first << endl;
    }
}

void ls_li() {
    for (const auto& entry : currentDirectory->children) {
        std::cout << getInode(entry.second) << "  "
                  << entry.second->permissions << "  "
                  << formatCreationTime(entry.second->creationTime) << "  "
                  << entry.first << std::endl;
    }
}

std::string formatCreationTime(std::time_t time) {
    struct std::tm * timeinfo = std::localtime(&time);

    std::string month;
    switch (timeinfo->tm_mon) {
        case 0:  month = "Jan"; break;
        case 1:  month = "Feb"; break;
        case 2:  month = "Mar"; break;
        case 3:  month = "Apr"; break;
        case 4:  month = "May"; break;
        case 5:  month = "Jun"; break;
        case 6:  month = "Jul"; break;
        case 7:  month = "Aug"; break;
        case 8:  month = "Sep"; break;
        case 9:  month = "Oct"; break;
        case 10: month = "Nov"; break;
        case 11: month = "Dec"; break;
        default: month = "Err"; break;
    }

    std::ostringstream oss;
    oss << std::setw(2) << std::setfill(' ') << month << " "
        << std::setw(2) << timeinfo->tm_mday << " "
        << std::setw(2) << std::setfill('0') << timeinfo->tm_hour << ":"
        << std::setw(2) << timeinfo->tm_min;

    return oss.str();
}


    void ls_R() {
        lsRecursive(currentDirectory, true, true, 0);
    }

    void rm(string name) {
        fs::path filePath = currentPath() / name;
        if (fs::exists(filePath)) {
            if (!fs::is_directory(filePath)) {
                fs::remove(filePath);
                Inode* file = currentDirectory->children[name];
                currentDirectory->children.erase(name);
                delete file;
            } else {
                cout << "Error: '" << name << "' is a directory." << endl;
            }
        } else {
            cout << "Error: File '" << name << "' not found." << endl;
        }
    }

    void rmdir(string name) {
        fs::path dirPath = currentPath() / name;
        if (fs::exists(dirPath)) {
            if (fs::is_directory(dirPath)) {
                removeRecursive(dirPath);
                Inode* directory = currentDirectory->children[name];
                currentDirectory->children.erase(name);
                delete directory;
            } else {
                cout << "Error: '" << name << "' is not a directory." << endl;
            }
        } else {
            cout << "Error: Directory '" << name << "' not found." << endl;
        }
    }

    void mv(string oldName, string newName) {
        fs::path oldPath = currentPath() / oldName;
        fs::path newPath = currentPath() / newName;
        if (fs::exists(oldPath)) {
            if (!fs::exists(newPath)) {
                fs::rename(oldPath, newPath);
                Inode* file = currentDirectory->children[oldName];
                currentDirectory->children.erase(oldName);
                file->name = newName;
                currentDirectory->children[newName] = file;
            } else {
                cout << "Error: A file or directory named '" << newName << "' already exists." << endl;
            }
        } else {
            cout << "Error: File or directory '" << oldName << "' not found." << endl;
        }
    }

    void chmod(string name, string permissions) {
        if (permissions.length() != 3 || !all_of(permissions.begin(), permissions.end(), ::isdigit)) {
            cout << "Error: Invalid permissions string format." << endl;
            return;
        }

        int ownerPerms = permissions[0] - '0';
        int groupPerms = permissions[1] - '0';
        int otherPerms = permissions[2] - '0';

        if (ownerPerms < 0 || ownerPerms > 7 || groupPerms < 0 || groupPerms > 7 || otherPerms < 0 || otherPerms > 7) {
            cout << "Error: Invalid permissions values." << endl;
            return;
        }

        mode_t mode = (ownerPerms << 6) | (groupPerms << 3) | otherPerms;
        fs::path filePath = currentPath() / name;
        if (fs::exists(filePath)) {
            ::chmod(filePath.c_str(), mode);

            if (currentDirectory->children.find(name) != currentDirectory->children.end()) {
                Inode* file = currentDirectory->children[name];
                file->permissions = toPermissionString(mode, file->isDirectory);
            }
        } else {
            cout << "Error: File or directory '" << name << "' not found." << endl;
        }
    }

    void direc() {
        string prompt = getFullPath(currentDirectory);
        cout << "~" << prompt << "$ ";
    }

    bool findInode(Inode* directory, const string& name, bool searchFile, bool searchDirectory, string& path) {
        if ((searchFile && !directory->isDirectory && directory->name == name) ||
            (searchDirectory && directory->isDirectory && directory->name == name)) {
            path = getFullPath(directory);
            return true;
        }

        for (auto& child : directory->children) {
            if (findInode(child.second, name, searchFile, searchDirectory, path)) {
                return true;
            }
        }
        return false;
    }

    void find(string type, string name) {
        bool searchFile = (type == "f");
        bool searchDirectory = (type == "d");

        string path;
        if (findInode(root, name, searchFile, searchDirectory, path)) {
            cout << "Found: " << path << endl;
        } else {
            cout << "Error: " << (searchFile ? "File" : "Directory") << " '" << name << "' not found." << endl;
        }
    }

private:
    Inode* root;
    Inode* currentDirectory;
    fs::path rootPath;

    fs::path currentPath() {
        return rootPath / fs::relative(getFullPath(currentDirectory), "/");
    }

    string getFullPath(Inode* inode) {
        if (inode->parent == nullptr) {
            return "";
        }
        return getFullPath(inode->parent) + "/" + inode->name;
    }

    void lsRecursive(Inode* directory, bool includeFiles, bool includeDirectories, int level) {
        if (includeDirectories) {
            for (int i = 0; i < level; ++i) {
                cout << "  ";
            }
            cout << directory->name << endl;
        }

        for (const auto& child : directory->children) {
            if (child.second->isDirectory) {
                lsRecursive(child.second, includeFiles, includeDirectories, level + 1);
            } else if (includeFiles) {
                for (int i = 0; i < level + 1; ++i) {
                    cout << "  ";
                }
                cout << child.second->name << endl;
            }
        }
    }

    void deleteInode(Inode* inode) {
        for (auto& child : inode->children) {
            deleteInode(child.second);
        }
        delete inode;
    }

    string toPermissionString(mode_t mode, bool isDirectory) {
        string permissions = isDirectory ? "d" : "-";
        for (int i = 2; i >= 0; --i) {
            int value = (mode >> (i * 3)) & 0b111;
            permissions += (value & 0b100) ? "r" : "-";
            permissions += (value & 0b010) ? "w" : "-";
            permissions += (value & 0b001) ? "x" : "-";
        }
        return permissions;
    }

    int getInode(Inode* inode) {
    static int inodeCounter = 1;
    static std::map<Inode*, int> inodeMap;

    if (inodeMap.find(inode) == inodeMap.end()) {
        inodeMap[inode] = inodeCounter++;
    }

    return inodeMap[inode];
}

    void removeRecursive(const fs::path& path) {
        if (fs::exists(path)) {
            fs::remove_all(path);
        }
    }

    void mapFileSystem(const fs::path& path, Inode* parentNode) {
    for (const auto& entry : fs::directory_iterator(path)) {
        string name = entry.path().filename().string();
        bool isDir = entry.is_directory();
        string permissions = getPermissions(entry.path());
        std::filesystem::file_time_type fileTime = fs::last_write_time(entry.path());
        auto timePoint = std::chrono::time_point_cast<std::chrono::system_clock::duration>(fileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        std::time_t creationTime = std::chrono::system_clock::to_time_t(timePoint);
        Inode* node = new Inode(name, isDir, permissions, parentNode);
        node->creationTime = creationTime; // Set creation time
        parentNode->children[name] = node;

        if (isDir) {
            mapFileSystem(entry.path(), node); // Recursively map directories
        }
    }
}


    string getPermissions(const fs::path& path) {
        struct stat info;
        if (stat(path.c_str(), &info) != 0) {
            return "";
        }

        string permissions;
        permissions += (S_ISDIR(info.st_mode)) ? "d" : "-";
        permissions += (info.st_mode & S_IRUSR) ? "r" : "-";
        permissions += (info.st_mode & S_IWUSR) ? "w" : "-";
        permissions += (info.st_mode & S_IXUSR) ? "x" : "-";
        permissions += (info.st_mode & S_IRGRP) ? "r" : "-";
        permissions += (info.st_mode & S_IWGRP) ? "w" : "-";
        permissions += (info.st_mode & S_IXGRP) ? "x" : "-";
        permissions += (info.st_mode & S_IROTH) ? "r" : "-";
        permissions += (info.st_mode & S_IWOTH) ? "w" : "-";
        permissions += (info.st_mode & S_IXOTH) ? "x" : "-";

        return permissions;
    }

    string formatTime(std::time_t time) {
        char buffer[26];
        struct tm* tm_info;

        tm_info = localtime(&time);
        strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);

        return string(buffer);
    }
};

int main() {
    FileSystem fs;
    
    string command;
    while (true) {
        fs.direc();
        getline(cin, command);
        istringstream iss(command);
        vector<string> tokens{istream_iterator<string>{iss}, istream_iterator<string>{}};

        if (tokens.empty()) continue;

        const string& cmd = tokens[0];
        if (cmd == "mkdir" && tokens.size() == 2) {
            fs.mkdir(tokens[1]);
        } else if (cmd == "touch" && tokens.size() == 2) {
            fs.touch(tokens[1]);
        } else if (cmd == "cd" && tokens.size() == 2) {
            fs.cd(tokens[1]);
        } else if (cmd == "ls") {
            fs.ls();
        } else if (cmd == "ls-l") {
            fs.ls_l();
        } else if (cmd == "ls-li") {
            fs.ls_li();
        } else if (cmd == "ls-R") {
            fs.ls_R();
        } else if (cmd == "rm" && tokens.size() == 2) {
            fs.rm(tokens[1]);
        } else if (cmd == "rmdir" && tokens.size() == 2) {
            fs.rmdir(tokens[1]);
        } else if (cmd == "mv" && tokens.size() == 3) {
            fs.mv(tokens[1], tokens[2]);
        } else if (cmd == "chmod" && tokens.size() == 3) {
            fs.chmod(tokens[1], tokens[2]);
        } else if (cmd == "find" && tokens.size() == 3) {
            fs.find(tokens[1], tokens[2]);
        } else if (cmd == "exit") {
            break;
        } else {
            cout << "Error: Unknown command or incorrect usage." << endl;
        }
    }

    return 0;
}
