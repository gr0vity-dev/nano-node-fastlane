FROM ubuntu:22.04 as builder

ARG COMPILER=gcc
ARG NETWORK=live

# Install build dependencies
COPY ./ci/prepare/linux /tmp/prepare
RUN /tmp/prepare/prepare.sh

COPY ./ /tmp/src
WORKDIR /tmp/src

#Define ARGs for ci/build-node.sh
ARG CI_TAG=DEV_BUILD
ARG BUILD_TYPE=RelWidthDebInfo
ARG NANO_TEST=OFF
ARG NANO_GUI=OFF
ARG COVERAGE=OFF
ARG CI_VERSION_PRE_RELEASE=OFF
ARG CMAKE_SANITIZER=""
ARG CMAKE_QT_DIR=""
ARG CMAKE_BACKTRACE=""


# Build node
RUN ci/build-node.sh
RUN echo ${NETWORK} >/etc/nano-network

FROM ubuntu:22.04

RUN groupadd --gid 1000 nanocurrency && \
useradd --uid 1000 --gid nanocurrency --shell /bin/bash --create-home nanocurrency

COPY --from=builder /tmp/src/build/nano_node /usr/bin
COPY --from=builder /tmp/src/build/nano_rpc /usr/bin
COPY --from=builder /tmp/src/api/ /usr/bin/api/
COPY --from=builder /etc/nano-network /etc

COPY docker/node/entry.sh /usr/bin/entry.sh
COPY docker/node/config /usr/share/nano/config
RUN chmod +x /usr/bin/entry.sh
RUN ln -s /usr/bin/nano_node /usr/bin/rai_node
RUN ldconfig

WORKDIR /root
USER root

ENV PATH="${PATH}:/usr/bin"
ENTRYPOINT ["/usr/bin/entry.sh"]
CMD ["nano_node", "daemon", "-l"]

ARG REPOSITORY=nanocurrency/nano-node
LABEL org.opencontainers.image.source https://github.com/$REPOSITORY
