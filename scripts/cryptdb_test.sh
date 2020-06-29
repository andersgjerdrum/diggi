docker pull mycrypt/cryptdb

docker run -d -P --name cdb --tmpfs /app mycrypt/cryptdb
docker exec -it cdb bash


service mysql start
cryptdb.sh start

mysqladmin create tpcc1000 -uroot -pletmein -h127.0.0.1 -P3306

git clone https://github.com/Percona-Lab/tpcc-mysql.git

cd tpcc-mysql/src/

make

cd ..

mysql tpcc1000 -uroot -pletmein -h127.0.0.1 -P3306 < create_table.sql 

mysql tpcc1000 -uroot -pletmein -h127.0.0.1 -P3306 < add_fkey_idx.sql

./tpcc_load -d tpcc1000 -uroot -p "letmein" -h127.0.0.1 -P3306 -w 2

./tpcc_start -d tpcc1000 -uroot -p "letmein" -h127.0.0.1 -P3306 -w 2 -c128 -r10 -l10800
