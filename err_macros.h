#define POST_WARN(str) std::cerr << "(" << time(NULL) << ") " << "\e[33m" << "Warning: " << str << "\e[0m" << std::endl
#define POST_ERR(str) std::cerr << "(" << time(NULL) << ") " << "\e[1;31m" << "Error: " << str << "\e[0m" << std::endl
#define POST_INFO(str) std::cerr << "(" << time(NULL) << ") " << "\e[32m" << str << "\e[0m" << std::endl


