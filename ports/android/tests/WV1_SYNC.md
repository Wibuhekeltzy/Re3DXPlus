# WV1 upstream sync: network tests and limitations

The Android network client and embedded host are byte-for-byte copies of `src/{net.c,net.h,vs_host.c,vs_host.h}` at upstream commit 76e1558; `tools/vs_server.py` is copied from the same commit. The wire version is WV1, incompatible with WV2.

Newly added historical WV2-specific fixtures, including the old CLOSED/ABORT extraction fixture, are excluded from this branch because they do not apply to the synced WV1 code. The baseline files `net_state_synthetic.c` and `net_udp_loopback.c` are retained for history but also target WV2 and do not test the current WV1 implementation. Separate junction UI and barrier tests remain. The WV2 abort case in `test_story_goal_freeze_host.py` is explicitly skipped under WV1, while its close and story cases remain active. Historical WV2 tests are not evidence of WV1 functionality.

Run the local embedded-host smoke test from the repository root:

```sh
cc -std=c11 -Iports/android/src ports/android/test/net_protocol_v1_test.c \
  ports/android/src/net.c ports/android/src/vs_host.c -o /tmp/net_protocol_v1_test
/tmp/net_protocol_v1_test
```

To test the upstream Python server instead, run `python3 ports/android/tools/vs_server.py` in a separate terminal and then run `/tmp/net_protocol_v1_test server`; stop the server afterward. Both tests bind UDP 27015 and need it free. They check two-player HELLO/LOBBY, START/GO and race relay, plus WV2 discovery rejection; the embedded-host test also checks WV1 discovery. GameKit is stubbed; these tests do not exercise Android, actual Wi-Fi, packet loss, timing races or hostile traffic.

WV1 does not provide WV2 session/join nonces, generation acknowledgements, explicit abort/done coordination, or reliable GO retransmission. The upstream host repeats GO three times, and client host-closed state is signaled by CLOSED rather than an automatic client timeout. These limitations are not equivalent to WV2 guarantees.
