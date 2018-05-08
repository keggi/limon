<?php
	
	require_once('../../config/secrets_db.php');

	// Sanity check the request

	if($_SERVER['REQUEST_METHOD'] !== 'POST') {
		http_response_code(400);
		exit();
	}

	if(!isset($_POST['id']) || !isset($_POST['channel']) || !isset($_POST['reading'])) {
		http_response_code(400);
		exit();
	}

	// Validate the token

	$headers = getallheaders();
    if(!array_key_exists('X-Device-Authorization', $headers)) {
		http_response_code(400);
		exit();
    }

	// SCHEMA: id, device_id, channel, reading, timestamp_received
    $mysql_pdo = new PDO($mysql_pdodsn, $mysql_username, $mysql_password, $mysql_pdoopt);

	$sql = 'SELECT token FROM devices WHERE id=? LIMIT 1';
	$sql_params = [$_POST['id']];
	$query = $mysql_pdo->prepare($sql);
	$query->execute($sql_params);
	$result = $query->fetch();

	$exp_token = 'Bearer ' . $result['token'];

	if(strtolower($headers['X-Device-Authorization']) !== strtolower($exp_token)) {
		http_response_code(403);
		exit();
	}

	// Save values
	$sql = '
        INSERT INTO readings
 			(device_id, channel, reading)
        VALUES
        	(?, ?, ?);';
    $sql_params = [$_POST['id'], $_POST['channel'], $_POST['reading']];
	$mysql_pdo->prepare($sql)->execute($sql_params);

	http_response_code(200);

?>