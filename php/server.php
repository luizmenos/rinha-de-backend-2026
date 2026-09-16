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
    $response->header("Content-Type", "text/plain");
    $response->header("X-Backend", gethostname());
    $response->end("Hello World\n");
});

$server->start();
