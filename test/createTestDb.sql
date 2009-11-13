drop database if exists mysql_icu_test;
create database mysql_icu_test;
grant select,create,insert,update,delete,index,drop on mysql_icu_test.* to 'mysqlicu'@'localhost' identified by 'icu4mysql';

