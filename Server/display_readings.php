<?php
	
	$page_token = 'somereasonablylonghexstringgeneratedrandomly';
	
	// Basically if you have the token you can view the page
	// display_readings.php?did=3&ch=0&token=somereasonablylonghexstringgeneratedrandomly

	require_once('../config/secrets_db.php');

	// Sanity check the request

	if($_SERVER['REQUEST_METHOD'] !== 'GET') {
		http_response_code(400);
		exit();
	}

	if(!isset($_GET['token']) || !isset($_GET['did']) || !isset($_GET['ch'])) {
		http_response_code(400);
		exit();
	}

	// Validate the token
	if(strtolower($_GET['token']) !== strtolower($page_token)) {
		http_response_code(403);
		exit();
	}

	// Get value
	// SCHEMA: id, device_id, channel, reading, timestamp_received
    $mysql_pdo = new PDO($mysql_pdodsn, $mysql_username, $mysql_password, $mysql_pdoopt);

	$sql = '
		SELECT
			*
		FROM (
			SELECT
				DATE_FORMAT(timestamp_received, "%Y-%m-%d %H:00:00") as received_at,
				600 - AVG(reading) as reading
			FROM readings
			WHERE
				device_id = ? AND
				channel = ? AND
				timestamp_received >= DATE_ADD(NOW(), INTERVAL -28 DAY)
			GROUP BY 1
			ORDER BY 1
		) agg
		WHERE agg.reading >= 0';
	$sql_params = [$_GET['did'], $_GET['ch']];
	$query = $mysql_pdo->prepare($sql);
	$query->execute($sql_params);
	$result = $query->fetchAll();

	$json=json_encode($result);
	header('Content-type: application/json');
	echo $json;

?>