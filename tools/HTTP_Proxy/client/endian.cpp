#include <iostream>

int main() {
    int num = 1;
    // 将int指针转为char指针，检查第一个字节
    if (*(char *)&num == 1) {
        std::cout << "Little-Endian" << std::endl;
    } else {
        std::cout << "Big-Endian" << std::endl;
    }
    return 0;
}