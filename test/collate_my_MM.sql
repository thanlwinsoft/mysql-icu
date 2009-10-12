-- Test ICU charset collation
set names utf8;

drop database if exists collate_my_MM;
create database collate_my_MM;
use collate_my_MM;
create table testCollate (
    id INT NOT NULL PRIMARY KEY AUTO_INCREMENT,
    word VARCHAR(128)
) collate utf8_icu_my_MM_ci;
insert into testCollate (word) values ('ခါ'),('ကို'),('ကာ'),('ကူ'),('ကီ'),('ကု'),('ခို'),('ခ'),('ကြ'),('ကြွ'),('ချ'),('ခှ'),('ကျ'),('ကှ'),('ကံ'),('ကက်'),('ခေါ်');
-- select * from testCollate order by word into outfile '/home/keith/projects/mysql-icu/mysql-icu/test/collate_my_MM.result1';
-- select * from testCollate order by word collate utf8_icu_custom_ci into outfile '/home/keith/projects/mysql-icu/mysql-icu/test/collate_my_MM.result2';

create table ordered_my_MM (
    id INT NOT NULL PRIMARY KEY AUTO_INCREMENT,
    word VARCHAR(128)
) collate utf8_icu_my_MM_ci;
insert into ordered_my_MM (word) select word from testCollate order by word;

create table ordered_custom (
    id INT NOT NULL PRIMARY KEY AUTO_INCREMENT,
    word VARCHAR(128)
) collate utf8_icu_my_MM_ci;
insert into ordered_custom (word) select word from testCollate order by word collate utf8_icu_custom_ci;

-- This shouldn't output anything if the custom is using the same rules as my_MM
select ordered_my_MM.id, ordered_custom.id, ordered_my_MM.word  from ordered_my_MM left join ordered_custom on (ordered_my_MM.word = ordered_custom.word) where ordered_my_MM.id != ordered_custom.id;

create table ordered_uca (
    id INT NOT NULL PRIMARY KEY AUTO_INCREMENT,
    word VARCHAR(128)
) collate utf8_icu_my_MM_ci;
insert into ordered_uca (word) select word from testCollate order by word collate utf8_unicode_ci;

-- This will output the differences between Myanmar and Unicode collation
select ordered_my_MM.id as my_MM, ordered_uca.id as uca, ordered_my_MM.word  from ordered_my_MM left join ordered_uca on (ordered_my_MM.word = ordered_uca.word) where ordered_my_MM.id != ordered_uca.id;

-- Now test using UTF16 charset
create table testCollate16 (
    id INT NOT NULL PRIMARY KEY AUTO_INCREMENT,
    word VARCHAR(128)
) collate ucs2_icu_custom_ci;
insert into testCollate16 (word) values ('ခါ'),('ကို'),('ကာ'),('ကူ'),('ကီ'),('ကု'),('ခို'),('ခ'),('ကြ'),('ကြွ'),('ချ'),('ခှ'),('ကျ'),('ကှ'),('ကံ'),('ကက်'),('ခေါ်');
select word from testCollate16 order by word collate ucs2_icu_custom_ci;

