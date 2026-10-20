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

$socketPath = getenv('SOCKET_PATH') ?: '/run/sockets/api.sock';
$socketDir = dirname($socketPath);
if (!is_dir($socketDir) && !mkdir($socketDir, 0770, true) && !is_dir($socketDir)) {
    throw new RuntimeException("Could not create socket directory: {$socketDir}");
}
if (file_exists($socketPath) || is_link($socketPath)) {
    unlink($socketPath);
}

umask(0007);
$server = new Server($socketPath, 0, SWOOLE_PROCESS, SWOOLE_SOCK_UNIX_STREAM);
$server->set([
    'worker_num' => 1,
    'reactor_num' => 1,
    'http_compression' => false,
]);

$server->on("Start", function () use ($socketPath) {
    echo "Server started on unix socket {$socketPath}\n";
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
