#define POST_WARN(str) std::cout << "(" << getpid() << ") " << "\e[33m" << "Warning: " << str << "\e[0m" << std::endl
#define POST_ERR(str) std::cerr << "(" << getpid() << ") " << "\e[1;31m" << "Error: " << str << "\e[0m" << std::endl
#define POST_INFO(str) std::cout << "(" << getpid() << ") " << "\e[32m" << str << "\e[0m" << std::endl


