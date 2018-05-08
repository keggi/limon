<?php

	// DB Connection Keys - Replace these with your own
    $mysql_hostname = 'mysql.YOURDOMAIN.com';
    $mysql_username = 'MYSQL_USERNAME';
    $mysql_password = 'MYSQL_PASSWORD';
    $mysql_database = 'MYSQL_DB_NAME';
    $mysql_charset = 'utf8';

    $mysql_pdodsn = "mysql:host=$mysql_hostname;dbname=$mysql_database;charset=$mysql_charset";
	$mysql_pdoopt = [
		    PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
		    PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
		    PDO::ATTR_EMULATE_PREPARES   => false,
		];

?>