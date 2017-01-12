qemu-net
========

`qemu-net` is a utility that simplifies the process of creating and deleting
TUN/TAP interfaces--and adding those interfaces to an existing bridge--intended
to be used with [QEMU](#). `qemu-net` is designed to be setuid, allowing
non-root users to create the necessary networking infrastructure for their
QEMU-based virtual machines.

QEMU Usage
==========

    qemu-system-x86_64 \
        -cdrom foo.iso \
        -enable-kvm \
        -hda sda \
        -boot order=dc \
        -m 256M \
        -smp 4 \
        -netdev tap,id=hn0 \
        -device virtio-net-pci,netdev=hn0,id=nic1,mac=FA:CE:00:00:00:0${VNC} \
        -vnc ":2"

TODO
====

- Support creation/deletion via a configuration file.

- Introduce a (*very simple*) method for controlling what interfaces can be
  manipulated by what users.

