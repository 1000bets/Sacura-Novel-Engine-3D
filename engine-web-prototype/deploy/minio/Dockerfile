# Upstream no longer distributes the community images. Build the pinned source release.
FROM golang:1.25-alpine AS build
RUN apk add --no-cache git
WORKDIR /src
RUN git clone --depth 1 --branch RELEASE.2025-10-15T17-29-55Z https://github.com/minio/minio.git .
RUN --mount=type=cache,target=/go/pkg/mod --mount=type=cache,target=/root/.cache/go-build CGO_ENABLED=0 go build -trimpath -ldflags='-s -w' -o /minio .
FROM alpine:3.22
RUN apk add --no-cache ca-certificates && addgroup -g 1000 minio && adduser -D -u 1000 -G minio minio && mkdir /data && chown minio:minio /data
COPY --from=build /minio /usr/local/bin/minio
USER minio
EXPOSE 9000
HEALTHCHECK --interval=10s --timeout=5s --start-period=30s CMD wget -q -O /dev/null http://127.0.0.1:9000/minio/health/live || exit 1
ENTRYPOINT ["minio"]
CMD ["server", "/data"]
