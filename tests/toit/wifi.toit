// Copyright (C) 2026 Toit contributors.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.

import net.wifi

SSID ::= "Open Wifi"
HOST ::= "192.168.4.2"
PORT ::= 18_080

main:
  print "TOIT-QEMU-WIFI: scanning"
  access-points := wifi.scan #[4]
  found := false
  access-points.do: |access-point/wifi.AccessPoint|
    print "TOIT-QEMU-WIFI: found $access-point.ssid"
    if access-point.ssid == SSID: found = true
  if not found: throw "Simulated access point not found"

  network := wifi.open --ssid=SSID --password=""
  print "TOIT-QEMU-WIFI: connected $network.address"

  socket := network.tcp-connect HOST PORT
  try:
    socket.out.write "GET / HTTP/1.0\r\nHost: qemu.test\r\n\r\n" --flush
    response := socket.in.read-all
    status-end := response.index-of '\r'
    print "TOIT-QEMU-WIFI: response=$(response[0..status-end].to-string)"
  finally:
    socket.close
    network.close

  print "TOIT-QEMU-WIFI: PASS"
