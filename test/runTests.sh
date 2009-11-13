#!/bin/bash

export MYSQL='mysql --defaults-file=/usr/local/mysql/etc/my.cnf -u mysqlicu -picu4mysql'

function runtest ()
{
	if $MYSQL < $1.sql > $1.out;
	then
		if diff -u $1.expected $1.out;
		then
			echo $1 OK;
		else
			echo $1 output unexpected;
			exit 2;
		fi
	else
		cat $1.out;
		exit 1;
	fi
}

runtest collate_my_MM;
runtest collate_my_MMucs2;
runtest likeTest;
runtest likeTestUcs2;
runtest ftMatch;
# the icu parser can't actually be used with ucs2, so this doesn't do much
runtest ftMatchUcs2;

