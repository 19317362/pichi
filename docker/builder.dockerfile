FROM alpine:3.8

RUN sed -i 's/v3\.8/edge/g' /etc/apk/repositories && \
  apk add --no-cache g++ cmake make mbedtls-dev libsodium-dev rapidjson-dev libmaxminddb-dev \
  boost-context boost-dev boost-program_options boost-system boost-unit_test_framework
