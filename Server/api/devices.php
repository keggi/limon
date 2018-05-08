<?php

	require_once('../../config/secrets_db.php');
	require_once('../../config/secrets_devices.php');

	// Sanity check the request

	if($_SERVER['REQUEST_METHOD'] !== 'POST') {
		http_response_code(400);
		exit();
	}

	if(!isset($_POST['id'])) {
		http_response_code(400);
		exit();
	}

	$headers = getallheaders();
    if(!array_key_exists('X-Device-Secret', $headers)) {
		http_response_code(400);
		exit();
    }

	// Validate the secret

	$exp_secret = 'Bearer ' . $device_secret;
	if(strtolower($headers['X-Device-Secret']) !== strtolower($exp_secret)) {
		http_response_code(403);
		exit();
	}

	// SCHEMA: id, token, attached_at

	$mysql_pdo = new PDO($mysql_pdodsn, $mysql_username, $mysql_password, $mysql_pdoopt);

	// Check if the device is already registered
	$sql = 'SELECT id, token FROM devices WHERE id=? LIMIT 1';
	$sql_params = [$_POST['id']];
	$query = $mysql_pdo->prepare($sql);
	$query->execute($sql_params);
	$result = $query->fetch();
	if($query->rowCount() > 0) {
		$token = $result['token'];
		$id = $result['id'];
	}
	else {
		$token = bin2hex(openssl_random_pseudo_bytes(32));

		$sql = '
	        INSERT INTO devices
	 			(token)
	        VALUES
	        	(?);';
	    $sql_params = [$token];
		$mysql_pdo->prepare($sql)->execute($sql_params);

	  	$sql = 'SELECT LAST_INSERT_ID() as id;';
	  	$query = $mysql_pdo->prepare($sql);
	  	$query->execute([]);
		$result = $query->fetch();

		$id = $result['id'];

	}
	
	$data['id'] = (string) $id;
	$data['token'] = strtoupper($token);

	// Return ID, Token
	header('Content-Type: application/json');
	echo json_encode($data);

?>