FROM php:8.5-cli

RUN apt-get update && apt-get install -y \
    build-essential \
    libssl-dev \
    libbrotli-dev \
    && pecl install swoole \
    && docker-php-ext-enable swoole \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /php

COPY /php/ /php/

EXPOSE 8080

CMD [ "php", "server.php" ]
