#include <iostream>
#include <string>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <sstream>
using namespace std;

/*
pthread_create()
第三个参数是线程要执行函数的地址(函数指针)
在使用类成员函数作为工作函数时需要注意,
使用类成员函数时,this指针会被默认传进去,导致和(void*)不匹配,不能编译
因此,使用类成员函数时,只能使用静态成员函数,因此类的静态成员函数没有this指针
*/
// class A
// {
// public:
// 	void *pthrea_test(void *ptr)
// 	{
// 		while (1)
// 		{
// 			printf("I am one\n");
// 		}
// 	}
// 	static void *pthrea_test_statoic(void *ptr)
// 	{
// 		while (1)
// 		{
// 			printf("I am one\n");
// 		}
// 	}
// };
// int test()
// {
// 	string s = "2020-07-11 16:19:29.772159";
// 	cout << "len(s) = " << sizeof(s) << endl;
// 	cout << "len(s) = " << s.size() << endl;
// 	time_t cur = time(NULL);
// 	cout << "cur = " << cur << endl;
// 	return 0;
// }

int solution_1()
{
	int n = 0;
	cin >> n;
	vector<int> dp(n + 1, 0);
	dp[0] = 0;
	int cnt = 0;
	for (int i = 1; i <= n; ++i)
	{
		dp[i] = dp[i & (i - 1)] + 1;
		cnt += dp[i];
	}

	cout << "cnt = " << cnt << endl;
	return cnt;
}

int solution_2(vector<int> nums)
{
	int n = nums.size();
	if (0 == n)
		return 0;
	if (1 == n)
		return max(0, nums[0]);

	vector<int> dp(n, 0);

	dp[0] = max(0, nums[0]);
	dp[1] = max(0, max(nums[0], nums[1]));

	for (int i = 2; i < n; ++i)
	{
		dp[i] = max(dp[i - 2] + nums[i], dp[i - 1]);
	}

	for (auto d : dp)
		cout << d << "  ";
	cout << endl;

	return dp[n - 1];
}

/* 把一个数分成三个正整数且使得乘积最大*/
int solution_3()
{
	int n;
	cin >> n;
	if (n <= 2)
		return 0;
	else if (n / 3 == 0)
		return pow(n / 3, 3);
	else if (n % 3 == 1)
		return pow((n - 1) / 3, 2) * (n + 2) / 3;
	else if (n % 3 == 2)
		return pow((n +1) / 3, 2) * (n-2)/3;

	int num1 = 0, num2 = 0, num3 = 0;
	

	// if (n <= 2)
	// 	return 0;
	// while (n > 0)
	// {
	// 	if (n > 0)
	// 	{
	// 		num1 += 1;
	// 		n -= 1;
	// 	}
	// 	else
	// 	{
	// 		break;
	// 	}

	// 	if (n > 0)
	// 	{
	// 		num2 += 1;
	// 		n -= 1;
	// 	}
	// 	else
	// 	{
	// 		break;
	// 	}

	// 	if (n > 0)
	// 	{
	// 		num3 += 1;
	// 		n -= 1;
	// 	}
	// 	else
	// 	{
	// 		break;
	// 	}
	// }

	return num1 * num2 * num3;
}

int main()
{
	cout << solution_3() << endl;

	return 0;
}
int main3()
{
	// string str = "[-11,1,-9,5,5,-5,-25]";
	// string str;
	// getline(cin, str);
	// istringstream is(str.substr(1, str.size() - 1));
	// int inter;
	// char ch;
	// vector<int> data;
	// while (is >> inter)
	// {
	// 	data.push_back(inter);
	// 	is >> ch;
	// }
	vector<int> data{-11, 1, -9, 5, 5, -5, -25};
	cout << solution_2(data) << endl;

	return 0;
}

int main1()
{
	// pthread_t pid;
	// pthread_create(&pid, NULL, A::pthrea_test_statoic, NULL);
	// while (1)
	// {
	// 	printf("I am Two\n");
	// }
	// time_t cur = time(NULL);
	// cout <<"cur time = " << cur << endl;

	// vector<int> nums{-11, 1, -9, 5, 5, -5, -25};
	// string str = "[-11,1,-9,5,5,-5,-25]";
	string str;
	getline(cin, str);
	istringstream is(str.substr(1, str.size() - 1));
	int inter;
	char ch;
	vector<int> v;
	while (is >> inter)
	{
		v.push_back(inter);
		is >> ch;
	}
	for (int i = 0; i < v.size(); i++)
		cout << v[i] << "   ";
	cout << endl;

	cout << solution_2(v) << endl;

	return 0;
}
