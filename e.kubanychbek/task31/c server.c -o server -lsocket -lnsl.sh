#!/bin/sh

N=2               # сколько клиентов
BS=1024             # размер блока (bytes)
COUNT=1024          # сколько блоков => 1024*1024 = ~1 MiB на клиента
PATTERN="AbCdEfGhIjKlMnOpQrStUvWxYz0123456789"

i=1
while [ $i -le $N ]; do
  (
    # небольшой заголовок, чтобы в выводе сервера можно было видеть "кто"
    printf "client_%02d BEGIN\n" "$i"

    # основной поток данных (~1MiB): yes генерит бесконечно, dd отрежет нужный объём
    yes "$PATTERN" | dd bs=$BS count=$COUNT 2>/dev/null

    printf "\nclient_%02d END\n" "$i"
  ) | ./client >/dev/null 2>&1 &

  i=$((i + 1))
done

wait
echo "Done: $N clients, each sent about $((BS*COUNT)) bytes (+ headers)"
