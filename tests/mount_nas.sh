#!/bin/bash
USER=pi
sudo mkdir /mnt/audio
sudo chmod 775 /mnt/audio/
sudo mount -t cifs -o user=${USER},uid=$(id -u),gid=$(id -g) //192.168.3.242/audio /mnt/NAS/audio
