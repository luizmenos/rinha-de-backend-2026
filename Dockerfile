FROM php:8.5-cli

RUN apt-get update && apt-get install -y \
    build-essential \
    libssl-dev \
    libbrotli-dev \
    && pecl install swoole \
    && docker-php-ext-enable swoole \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

COPY ext/kdtree/ /usr/src/kdtree/

RUN cd /usr/src/kdtree \
    && phpize \
    && ./configure --enable-kdtree \
    && make \
    && make install \
    && docker-php-ext-enable kdtree

WORKDIR /php

COPY /php/ /php/

EXPOSE 8080

CMD [ "php", "server.php" ]
