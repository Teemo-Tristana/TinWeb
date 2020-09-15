server: main.cpp ../threadpool/threadpool.h ../http/http_conn.cpp ../http/http_conn.h ../lock/locker.h ../log/log.cpp ../log/log.h ../log/block_queue.h ../connectionPool/sql_connection_pool.cpp ../CGImysql/sql_connection_pool.h ../userdata/redis.cpp ../userdata/redis.h 
	g++ -g -o server   main.cpp  ../threadpool/threadpool.h ../http/http_conn.cpp ../http/http_conn.h ../lock/locker.h ../log/log.cpp ../log/log.h ../connectionPool/sql_connection_pool.cpp ../CGImysql/sql_connection_pool.h ../userdata/redis.cpp ../userdata/redis.h -lpthread -I /usr/local/include/hiredis -lhiredis -lpthread -lmysqlclient -std=c++11


clean:
	rm  -r server
