-- Test using like for matching
set names utf8;

use mysql_icu_test;

drop table if exists likeTest;
create table likeTest  (
    id INT NOT NULL PRIMARY KEY AUTO_INCREMENT,
    phrase VARCHAR(128)
) collate ucs2_icu_my_MM_ci;

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

select * from likeTest where phrase like '%';
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
select  '%က__';
select * from likeTest where phrase like '%က__';
select  '_ူ';
select * from likeTest where phrase like '_ူ';
select  '%မာ_ကား%';
select * from likeTest where phrase like '%မာ_ကား%';





