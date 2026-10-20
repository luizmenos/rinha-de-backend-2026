<?php

declare(strict_types=1);

require_once __DIR__ . '/../app/Vector.php';
require_once __DIR__ . '/../app/KDTree.php';

use Swoole\Http\Request;
use Swoole\Http\Response;
use Swoole\Http\Server;
use App\Vector;
use App\KDTree;

$vectorizer = new Vector();
$tree = new KDTree(__DIR__ . '/../resources/references.idx');

$server = new Server(host: "0.0.0.0", port: 8080);

$server->on("Start", function (Server $server) {
    echo "Server started at {$server->host}:{$server->port}";
});

$server->on("Request", function (Request $request, Response $response) use ($vectorizer, $tree) {
    $uri = $request->server['request_uri'] ?? '';
    $method = $request->server['request_method'] ?? '';

    if ($uri === '/ready' && $method === 'GET') {
        $response->status(200);
        $response->header("Content-Type", "text/plain");
        $response->end("OK\n");
        return;
    }

    if ($uri === "/fraud-score") {
        if ($method === 'POST') {
            $body = $request->rawContent();
            if (empty($body)) {
                $response->status(400);
                $response->end('');
                return;
            }

            $payload = json_decode($body, true);

            if (!is_array($payload)) {
                $response->status(400);
                $response->end('');
                return;
            }

            $query = $vectorizer->vectorize($payload);
            $fraudScore = $tree->fraudCount($query) / 5;
            $result = [
                'approved' => $fraudScore < 0.6,
                'fraud_score' => $fraudScore,
            ];

            $response->status(200);
            $response->header("Content-Type", "application/json");
            $response->end(json_encode($result));
            return;
        }

        $response->status(405);
        $response->end("Method Not Allowed\n");
        return;
    }

    $response->status(404);
    $response->end("");
});

$server->start();
