FROM php:8.5-cli AS index-builder

WORKDIR /app
COPY scripts/ /app/scripts/
COPY resources/references.json.gz /app/resources/references.json.gz
RUN php /app/scripts/build_index.php /app/resources/references.json.gz /app/resources/references.idx

FROM php:8.5-cli

RUN apt-get update && apt-get install -y \
    build-essential \
    libssl-dev \
    libbrotli-dev \
    && pecl install swoole \
    && docker-php-ext-enable swoole \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

COPY lib/ /usr/src/lib/
COPY ext/kdtree/ /usr/src/ext/kdtree/

RUN cd /usr/src/ext/kdtree \
    && phpize \
    && ./configure --enable-kdtree \
    && make \
    && make install \
    && docker-php-ext-enable kdtree

WORKDIR /app

COPY app/ /app/app/
COPY php/ /app/php/
COPY --from=index-builder /app/resources/references.idx /app/resources/references.idx

EXPOSE 8080

CMD [ "php", "php/server.php" ]
