#include <iostream>
#include <cstdarg>

using namespace std;

// int sum(int format, ...)
// {
//     if (format <= 0)
//         return 0;

//     va_list valist;
//     va_start(valist, format);

//     int sum = 0;
//     for(int i = 0; i < format; ++i)
//         sum += va_arg(valist, int);

//     va_end(valist);

//     return sum;
// }

int sum(int count, ...)
{
    
    if (count <= 0)
        return 0;
    va_list arg_ptr;
    va_start(arg_ptr, count);

    int sum = 0;
    for (int i = 0; i < count; i++)
        sum += va_arg(arg_ptr, int);
    va_end(arg_ptr);
    return sum;
}

int main()
{
    cout << sum(3, 1, 2, 3) << endl;
    cout << sum(5, 1, 2, 3, 4, 5) << endl;

    return 0;
}