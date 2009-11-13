-- Test using like for matching
set names utf8;
use mysql_icu_test;

drop table if exists likeTest;
create table likeTest  (
    id INT NOT NULL PRIMARY KEY AUTO_INCREMENT,
    phrase VARCHAR(128)
) collate utf8_icu_my_MM_ci;

insert into likeTest (phrase) values 
('မြန်မာစကား'),
('စကာလုံးများ'),
('ဘယ်နှလုံးလဲ။'),
('ဘာလုပ်မလဲ။'),
('မြန်မာဘာသာစကား'),
('မြန်မာလူမျို'),
('မြန်မာနိုင်ငံ'),
('နိုင်ငံရေး'),
('လူများတယ်'),
('အင်္ဂလိန်ကမြန်မာပြည်ကိုလားတယ်။');

select  '%လူ%';
select * from likeTest where phrase like '%လူ%';
select 'မ%';
select * from likeTest where phrase like 'မ%';
select 'မြ%';
select * from likeTest where phrase like 'မြ%';
select '%န%';
select * from likeTest where phrase like '%န%';
select '%န်%';
select * from likeTest where phrase like '%န်%';
select '%လုံး%';
select * from likeTest where phrase like '%လုံး%';
select '%မျ%';
select * from likeTest where phrase like '%မျ%';
select  '%များ';
select * from likeTest where phrase like '%များ';
select  '%မျ%';
select * from likeTest where phrase like '%မျ%';
select  '%မာ%။';
select * from likeTest where phrase like '%မာ%။';
select  '%မာ%ကား';
select * from likeTest where phrase like '%မာ%ကား';
select  '%မာ%ကို%';
select * from likeTest where phrase like '%မာ%ကို%';
select  'မြန်%မာ%';
select * from likeTest where phrase like 'မြန်%မာ%';
-- arguably this should match, but because ကား is a combined collation unit, it
-- doesn't.
select  '%စ___';
select * from likeTest where phrase like '%စ___';

select  '_ကာ%';
select * from likeTest where phrase like '_ကာ%';
select  '%မာ_ကား%';
select * from likeTest where phrase like '%မာ_ကား%';

-- this fails because ူ is not found on its own
select  '%ူ%';
select * from likeTest where phrase like '%ူ%';
-- if you need raw code point based searches use the default collation
select  'general %ူ%';
select * from likeTest where phrase like '%ူ%' collate utf8_general_ci;

