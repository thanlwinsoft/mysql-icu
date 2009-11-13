-- Test full text parsing
set names utf8;

use mysql_icu_test;

drop table if exists matchTest;
drop table if exists matchTest2;

create table matchTest  (
    id INT NOT NULL PRIMARY KEY AUTO_INCREMENT,
    phrase VARCHAR(128)
) collate ucs2_icu_my_MM_ci;
create table matchTest2  (
    id INT NOT NULL PRIMARY KEY AUTO_INCREMENT,
    phrase VARCHAR(128)
) collate ucs2_icu_custom_ci;

-- Fulltext indexes are not supported with UCS2, only boolean mode is supported

insert into matchTest2 (phrase) values 
('မြန်မာစကား'),
('စကာလုံးများ'),
('ဘယ်နှလုံးလဲ။'),
('ဘာလုပ်မလဲ။'),
('မြန်မာဘာသာစကား'),
('မြန်မာလူမျို'),
('မြန်မာနိုင်ငံ'),
('နိုင်ငံရေး'),
('လူများတယ်'),
('အင်္ဂလိန်ကမြန်မာပြည်ကိုလားတယ်။'),
('မြန်လားမာလား။');

insert into matchTest2 (phrase) values 
('This is English'),
('It is easier to use full text searches in English');

insert into matchTest select * from matchTest2;

repair table matchTest quick;
repair table matchTest2 quick;

-- Boolean mode can be used, but the parser can't be set!
select 'မြန်မာ*';
select * from matchTest where match (phrase) against ('မြန်မာ*' IN BOOLEAN MODE);
select 'လူ';
select * from matchTest where match (phrase) against ('လူ' IN BOOLEAN MODE);
select '+လူ';
select * from matchTest where match (phrase) against ('+လူ' IN BOOLEAN MODE);

select 'Custom rules';
select 'လုံး';
select * from matchTest2 where match (phrase) against ('လုံး' IN BOOLEAN MODE);
select '+လူ';
select * from matchTest2 where match (phrase) against ('+လူ' IN BOOLEAN MODE);

select 'မြန်မာစကား';
select * from matchTest2 where match (phrase) against ('မြန်မာစကား' IN BOOLEAN MODE);

select '+လူ -"မြန်မာ"';
select * from matchTest2 where match (phrase) against ('+လူ -မြန်' IN BOOLEAN MODE);

select '"မြန်မာ" boolean';
select * from matchTest2 where match (phrase) against ('"မြန်မာ"' IN BOOLEAN MODE);
select 'မြန်မာ*';
select * from matchTest2 where match (phrase) against ('မြန်မာ*' IN BOOLEAN MODE);

