qemu-net
========

`qemu-net` is a utility that simplifies the process of creating and deleting
TUN/TAP interfaces--and adding those interfaces to an existing bridge--intended
to be used with [QEMU](#). `qemu-net` is designed to be setuid, allowing
non-root users to create the necessary networking infrastructure for their
QEMU-based virtual machines.

TODO
====

- Support creation/deletion via a configuration file.

- Introduce a (*very simple*) method for controlling what interfaces can be
  manipulated by what users.

