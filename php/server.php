<?php

declare(strict_types=1);

use Swoole\Http\Request;
use Swoole\Http\Response;
use Swoole\Http\Server;

$server = new Server(host: "0.0.0.0", port: 8080);

$server->on("Start", function (Server $server) {
    echo "Server started at {$server->host}:{$server->port}";
});

$server->on("Request", function (Request $request, Response $response) {
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

            $response->status(200);
            $response->header("Content-Type", "application/json");
            /**
             * 
             * @todo return something like this:
             * 
             * {
             * "approved": false,
             * "fraud_score": 1.0
             * }
             * 
             */
            return;
        }

        $response->status(405);
        $response->end("Method Not Allowed\n");
        return;
    }

    $response->header("Content-Type", "text/plain");
    $response->header("X-Backend", gethostname());
    $response->end("Hello World\n");
});

$server->start();
