#ifndef CSLIBS_NDT_SERIALIZATION_FILESYSTEM_HPP
#define CSLIBS_NDT_SERIALIZATION_FILESYSTEM_HPP

#include <boost/filesystem.hpp>

namespace cslibs_ndt {
namespace common {
namespace serialization {
inline bool check_directory(const boost::filesystem::path &p)
{
    if (!boost::filesystem::exists(p)) {
        std::cerr << "Path '" << p.string() << "' does not exist.\n";
        return false;
    }
    if (!boost::filesystem::is_directory(p)) {
        std::cerr << "Path '" << p.string() << "' is not a directory.\n";
        return false;
    }

    return true;
}

inline bool check_directory_quiet(const boost::filesystem::path &p)
{
    return boost::filesystem::exists(p) && boost::filesystem::is_directory((p));
}

inline bool check_file(const boost::filesystem::path &p)
{
    if (!boost::filesystem::exists(p)) {
        std::cerr << "Path '" << p.string() << "' does not exist.\n";
        return false;
    }
    if (!boost::filesystem::is_regular_file(p)) {
        std::cerr << "Path '" << p.string() << "' is not a directory.\n";
        return false;
    }

    return true;
}

inline bool check_file_quiet(const boost::filesystem::path &p)
{
    return boost::filesystem::exists(p) && boost::filesystem::is_regular_file(p);
}

inline bool create_directory(const boost::filesystem::path &p)
{
    if (boost::filesystem::exists(p)) {
        if(!boost::filesystem::remove_all(p)) {
            std::cerr << "Error deleting folder '" << p << "'from filesystem.\n";
            return false;
        }
    }
    if (!boost::filesystem::create_directory(p)) {
        std::cerr << "Error creating folder '" << p << "' in filesystem, check your rights.\n";
        return false;
    }
    return true;
}
}
}
}

#endif // CSLIBS_NDT_SERIALIZATION_FILESYSTEM_HPP
