#!/bin/sh
set -eu

secret_file=/run/secrets/turn_shared_secret
runtime_config=/run/coturn/turnserver.conf

if [ ! -r "$secret_file" ]; then
    echo "TURN shared secret is unavailable" >&2
    exit 1
fi

secret=$(tr -d '\r\n' < "$secret_file")
if [ "${#secret}" -ne 64 ]; then
    echo "TURN shared secret must be 64 hexadecimal characters" >&2
    exit 1
fi
case "$secret" in
    *[!0-9a-fA-F]*)
        echo "TURN shared secret must be hexadecimal" >&2
        exit 1
        ;;
esac

if [ -z "${TURN_REALM:-}" ] || [ -z "${TURN_EXTERNAL_IP:-}" ]; then
    echo "TURN_REALM and TURN_EXTERNAL_IP are required" >&2
    exit 1
fi

umask 077
cp /etc/coturn/turnserver.base.conf "$runtime_config"
printf '\nstatic-auth-secret=%s\n' "$secret" >> "$runtime_config"

exec turnserver \
    -c "$runtime_config" \
    --realm="$TURN_REALM" \
    --external-ip="$TURN_EXTERNAL_IP"
